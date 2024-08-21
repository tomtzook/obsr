
#include <utility>

#include "debug.h"
#include "internal_except.h"
#include "util/general.h"
#include "client.h"

namespace obsr::net {

#define LOG_MODULE "client"

static constexpr auto connect_retry_time = std::chrono::milliseconds(1000);
static constexpr auto server_sync_time = std::chrono::milliseconds(1000);

client::client(const std::shared_ptr<io::nio_runner>& nio_runner, const std::shared_ptr<clock>& clock)
    : m_clock(clock)
    , m_state(state::idle)
    , m_mutex()
    , m_io(nio_runner, this)
    , m_conn_info()
    , m_parser()
    , m_serializer()
    , m_storage()
    , m_connect_retry_timer()
    , m_clock_sync_timer()
    , m_clock_sync_data()
{}

void client::attach_storage(std::shared_ptr<storage::storage> storage) {
    m_storage = std::move(storage);
}

void client::start(connection_info info) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::idle) {
        throw illegal_state_exception();
    }

    if (!m_storage) {
        throw illegal_state_exception();
    }

    m_storage->clear_net_ids();
    m_connect_retry_timer.stop();
    m_clock_sync_timer.stop();

    m_conn_info = info;
    m_state = state::opening;
}

void client::stop() {
    std::unique_lock lock(m_mutex);

    if (m_state == state::idle) {
        throw illegal_state_exception();
    }

    if (!m_io.is_stopped()) {
        m_io.stop();
    }

    m_state = state::idle;
}

void client::update() {
    std::unique_lock lock(m_mutex);

    if (m_state == state::idle) {
        // we aren't running even
        return;
    }

    if (m_clock_sync_timer.is_running() && m_clock_sync_timer.has_elapsed(server_sync_time)) {
        TRACE_DEBUG(LOG_MODULE, "requesting time sync from server");
        if (write_server_sync()) {
            m_clock_sync_timer.stop();
        }
    }

    switch (m_state) {
        case state::opening: {
            if (m_connect_retry_timer.is_running() && !m_connect_retry_timer.has_elapsed(connect_retry_time)) {
                break;
            }

            if (open_socket_and_start_connection()) {
                // successful
                m_connect_retry_timer.stop();
                m_state = state::connecting;
            } else {
                m_connect_retry_timer.start();
            }

            break;
        }
        case state::in_use: {
            m_storage->act_on_entries([this](const storage::storage_entry& entry) -> bool {
                const auto id = entry.get_net_id();

                bool continue_run = false;
                if (entry.has_flags(storage::flag_internal_deleted)) {
                    // entry was deleted

                    if (id == storage::id_not_assigned) {
                        continue_run = true;
                    } else {
                        continue_run = write_entry_deleted(entry);
                    }
                } else {
                    // entry value was updated
                    if (id == storage::id_not_assigned) {
                        continue_run = write_entry_created(entry);
                    } else {
                        continue_run = write_entry_updated(entry);
                    }
                }

                // we want to mark un-dirty and resume if we succeeded
                return continue_run;
            }, storage::flag_internal_dirty);
            break;
        }
        case state::in_handshake_time_sync:
            TRACE_DEBUG(LOG_MODULE, "need to request sync with server");
            if (write_server_sync()) {
                m_state = state::in_handshake_time_sync_request_sent;
            }
            break;
        case state::in_handshake_signal_server_ready:
            TRACE_DEBUG(LOG_MODULE, "need to report ready to server");
            if (write_handshake_ready()) {
                m_state = state::in_handshake;
            }
            break;
        case state::in_handshake_time_sync_request_sent:
        case state::in_handshake:
            // in this phase we do not send anything to the server, just receive data
        case state::connecting:
        case state::idle:
        default:
            break;
    }
}

void client::on_new_message(const message_header& header, const uint8_t* buffer, size_t size) {
    std::unique_lock lock(m_mutex);

    auto type = static_cast<message_type>(header.type);
    m_parser.set_data(type, buffer, size);
    m_parser.process();

    if (m_parser.is_errored()) {
        TRACE_ERROR(LOG_MODULE, "failed to parse incoming data, parser error=%d", m_parser.error_code());
        return;
    } else if (!m_parser.is_finished()) {
        TRACE_ERROR(LOG_MODULE, "failed to parse incoming data, parser did not finish");
        return;
    }

    auto parse_data = m_parser.data();
    switch (type) {
        case message_type::entry_create:
            invoke_shared_ptr<storage::storage, storage::entry_id, std::string_view, const value&>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_created,
                    parse_data.id,
                    parse_data.name,
                    parse_data.value);
            break;
        case message_type::entry_update:
            invoke_shared_ptr<storage::storage, storage::entry_id, const value&>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_updated,
                    parse_data.id,
                    parse_data.value);
            break;
        case message_type::entry_delete:
            invoke_shared_ptr<storage::storage, storage::entry_id>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_deleted,
                    parse_data.id);
            break;
        case message_type::entry_id_assign:
            invoke_shared_ptr<storage::storage, storage::entry_id, std::string_view>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_id_assigned,
                    parse_data.id,
                    parse_data.name);
            break;
        case message_type::handshake_finished:
            TRACE_DEBUG(LOG_MODULE, "server declared handshake is finished");
            m_state = state::in_use;
            m_clock_sync_timer.start();
            break;
        case message_type::time_sync_response: {
            m_clock_sync_data.remote_start = parse_data.start_time;
            m_clock_sync_data.remote_end = parse_data.end_time;
            m_clock->sync(m_clock_sync_data);

            const auto time = m_clock->now();
            TRACE_DEBUG(LOG_MODULE, "received time sync response from server: %lu", time.count());

            if (m_state == state::in_handshake_time_sync_request_sent) {
                if (write_handshake_ready()) {
                    TRACE_DEBUG(LOG_MODULE, "transitioning to handshake wait");
                    m_state = state::in_handshake;
                } else {
                    TRACE_DEBUG(LOG_MODULE, "transitioning to handshake ready report");
                    m_state = state::in_handshake_signal_server_ready;
                }
            } else {
                m_clock_sync_timer.start();
            }
            break;
        }
        case message_type::no_type:
        default:
            break;
    }
}

void client::on_connected() {
    std::unique_lock lock(m_mutex);

    TRACE_DEBUG(LOG_MODULE, "connected to server, starting first time sync");


    if (write_server_sync()) {
        m_state = state::in_handshake_time_sync_request_sent;
    } else {
        m_state = state::in_handshake_time_sync;
    }
}

void client::on_close() {
    std::unique_lock lock(m_mutex);

    m_connect_retry_timer.stop();
    m_clock_sync_timer.stop();

    m_state = state::opening;
}

bool client::open_socket_and_start_connection() {
    try {
        auto socket = std::make_shared<obsr::os::socket>();
        socket->setoption<os::sockopt_reuseport>(true);

        m_io.start(socket);
        m_io.connect(m_conn_info);

        return true;
    } catch (...) {
        if (!m_io.is_stopped()) {
            m_io.stop();
        }

        return false;
    }
}

bool client::write_entry_created(const storage::storage_entry& entry) {
    TRACE_DEBUG(LOG_MODULE, "writing entry created %d", entry.get_net_id());

    m_serializer.reset();

    value value{};
    entry.get_value(value);
    if (!m_serializer.entry_created(entry.get_net_id(), entry.get_path(), value)) {
        return false;
    }

    if (!m_io.write(static_cast<uint8_t>(message_type::entry_create), m_serializer.data(), m_serializer.size())) {
        return false;
    }

    return true;
}

bool client::write_entry_updated(const storage::storage_entry& entry) {
    TRACE_DEBUG(LOG_MODULE, "writing entry updated %d", entry.get_net_id());

    m_serializer.reset();

    value value{};
    entry.get_value(value);
    if (!m_serializer.entry_updated(entry.get_net_id(), value)) {
        return false;
    }

    if (!m_io.write(static_cast<uint8_t>(message_type::entry_update), m_serializer.data(), m_serializer.size())) {
        return false;
    }

    return true;
}

bool client::write_entry_deleted(const storage::storage_entry& entry) {
    TRACE_DEBUG(LOG_MODULE, "writing entry deleted %d", entry.get_net_id());

    m_serializer.reset();

    if (!m_serializer.entry_deleted(entry.get_net_id())) {
        return false;
    }

    if (!m_io.write(static_cast<uint8_t>(message_type::entry_delete), m_serializer.data(), m_serializer.size())) {
        return false;
    }

    return true;
}

bool client::write_server_sync() {
    TRACE_DEBUG(LOG_MODULE, "writing server sync request");

    const auto time = m_clock->now();
    m_clock_sync_data.us_start = time;

    m_serializer.reset();

    if (!m_serializer.time_sync_request(time)) {
        return false;
    }

    if (!m_io.write(
            static_cast<uint8_t>(message_type::time_sync_request),
            m_serializer.data(),
            m_serializer.size())) {
        return false;
    }

    return true;
}

bool client::write_handshake_ready() {
    TRACE_DEBUG(LOG_MODULE, "writing ready for handshake");

    if (!m_io.write(
            static_cast<uint8_t>(message_type::handshake_ready),
            nullptr,
            0)) {
        return false;
    }

    return true;
}

}
