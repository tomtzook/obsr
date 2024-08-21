
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
    , m_message_queue(this)
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
    m_message_queue.clear();

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
        m_message_queue.enqueue_time_sync_request();
        m_clock_sync_timer.stop();
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
        case state::in_use:
            process_storage();
            m_message_queue.process();
            break;
        case state::in_handshake_time_sync:
        case state::in_handshake:
            // in this phase we do not send anything to the server, just receive data
            m_message_queue.process();
            break;
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
            invoke_shared_ptr<storage::storage, storage::entry_id, std::string_view, const value&, std::chrono::milliseconds>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_created,
                    parse_data.id,
                    parse_data.name,
                    parse_data.value,
                    parse_data.time);
            break;
        case message_type::entry_update:
            invoke_shared_ptr<storage::storage, storage::entry_id, const value&, std::chrono::milliseconds>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_updated,
                    parse_data.id,
                    parse_data.value,
                    parse_data.time);
            break;
        case message_type::entry_delete:
            invoke_shared_ptr<storage::storage, storage::entry_id, std::chrono::milliseconds>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_deleted,
                    parse_data.id,
                    parse_data.time);
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

            if (m_state == state::in_handshake_time_sync) {
                TRACE_DEBUG(LOG_MODULE, "transitioning to handshake wait");
                m_message_queue.enqueue_handshake_ready();
                m_state = state::in_handshake;
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
    m_message_queue.clear();

    m_message_queue.enqueue_time_sync_request();
    m_state = state::in_handshake_time_sync;
}

void client::on_close() {
    std::unique_lock lock(m_mutex);

    m_connect_retry_timer.stop();
    m_clock_sync_timer.stop();

    m_state = state::opening;
}

std::chrono::milliseconds client::get_time_now() {
    return m_clock->now();
}

bool client::write(uint8_t type, const uint8_t* buffer, size_t size) {
    return m_io.write(type, buffer, size);
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

void client::process_storage() {
    m_storage->act_on_entries([this](const storage::storage_entry& entry) -> bool {
        const auto id = entry.get_net_id();

        if (id == storage::id_not_assigned) {
            obsr::value value{};
            entry.get_value(value);

            m_message_queue.enqueue_entry_create(id, entry.get_path(), value);

            return true;
        }

        if (entry.has_flags(storage::flag_internal_deleted)) {
            // entry was deleted
            m_message_queue.enqueue_entry_deleted(id);
        } else {
            // entry value was updated
            obsr::value value{};
            entry.get_value(value);

            m_message_queue.enqueue_entry_update(id, value);
        }

        // we want to mark un-dirty and resume if we succeeded
        return true;
    }, storage::flag_internal_dirty);
}

}
