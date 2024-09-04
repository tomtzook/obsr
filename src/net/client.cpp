
#include <mutex>

#include "debug.h"
#include "internal_except.h"
#include "util/general.h"

#include "client.h"

namespace obsr::net {

#define LOG_MODULE "network_client"

static constexpr auto connect_retry_time = std::chrono::milliseconds(1000);
static constexpr auto server_sync_time = std::chrono::milliseconds(1000);
static constexpr auto update_time = std::chrono::milliseconds(200);

network_client::network_client(std::shared_ptr<clock>& clock)
    : m_mutex()
    , m_state(state::idle)
    , m_storage(nullptr)
    , m_clock(clock)
    , m_looper(nullptr)
    , m_conn_info({"", 0})
    , m_update_timer_handle(empty_handle)
    , m_io()
    , m_parser()
    , m_message_queue() {
    m_io.on_connect([this]()->void {
        TRACE_DEBUG(LOG_MODULE, "connected to server, starting first time sync");
        m_message_queue.clear();

        const auto now = m_clock->now();
        m_message_queue.enqueue(out_message::time_sync_request(now), message_queue::flag_immediate);
        m_state = state::in_handshake_time_sync;
    });
    m_io.on_close([this]()->void {
        m_connect_retry_timer.stop();
        m_clock_sync_timer.stop();

        m_state = state::opening;
    });
    m_io.on_message([this](const message_header& header, const uint8_t* buffer, size_t size)->void {
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
                TRACE_DEBUG(LOG_MODULE, "ENTRY CREATE from server: id=%d, name=%s", parse_data.id, parse_data.name.c_str());
                invoke_sharedptr_nolock<storage::storage, storage::entry_id, std::string_view, const obsr::value&, std::chrono::milliseconds>(
                        m_storage,
                        &storage::storage::on_entry_created,
                        parse_data.id,
                        parse_data.name,
                        parse_data.value,
                        parse_data.send_time);
                break;
            case message_type::entry_update:
                TRACE_DEBUG(LOG_MODULE, "ENTRY UPDATE from server: id=%d", parse_data.id);
                invoke_sharedptr_nolock<storage::storage, storage::entry_id, const obsr::value&, std::chrono::milliseconds>(
                        m_storage,
                        &storage::storage::on_entry_updated,
                        parse_data.id,
                        parse_data.value,
                        parse_data.send_time);
                break;
            case message_type::entry_delete:
                TRACE_DEBUG(LOG_MODULE, "ENTRY DELETE from server: id=%d", parse_data.id);
                invoke_sharedptr_nolock<storage::storage, storage::entry_id, std::chrono::milliseconds>(
                        m_storage,
                        &storage::storage::on_entry_deleted,
                        parse_data.id,
                        parse_data.send_time);
                break;
            case message_type::entry_id_assign:
                TRACE_DEBUG(LOG_MODULE, "ENTRY ASSIGN from server: id=%d, name=%s", parse_data.id, parse_data.name.c_str());
                invoke_sharedptr_nolock<storage::storage, storage::entry_id, std::string_view>(
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
                if (m_clock->sync(parse_data.time_value, parse_data.send_time)) {
                    m_storage->on_clock_resync();
                }

                const auto time = m_clock->now();
                TRACE_DEBUG(LOG_MODULE, "received time sync response from server: %lu", time.count());

                if (m_state == state::in_handshake_time_sync) {
                    TRACE_DEBUG(LOG_MODULE, "transitioning to handshake wait");
                    m_message_queue.enqueue(out_message::handshake_ready());
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
    });
    m_message_queue.attach([this](uint8_t type, const uint8_t* buffer, size_t size)->bool {
        return m_io.write(type, buffer, size);
    });
}

void network_client::configure_target(connection_info info) {
    if (m_state != state::idle) {
        throw illegal_state_exception();
    }

    m_conn_info = info;
}

void network_client::attach_storage(std::shared_ptr<storage::storage> storage) {
    if (m_state != state::idle) {
        throw illegal_state_exception();
    }

    m_storage = std::move(storage);
}

void network_client::start(events::looper* looper) {
    // todo: sync?

    if (m_state != state::idle) {
        throw illegal_state_exception();
    }

    if (!m_storage) {
        throw illegal_state_exception();
    }

    if (m_conn_info.port == 0) {
        throw illegal_state_exception();
    }

    m_storage->clear_net_ids();
    m_connect_retry_timer.stop();
    m_clock_sync_timer.stop();
    m_message_queue.clear();

    m_looper = looper;
    m_state = state::opening;

    auto update_callback = [this](events::looper&, obsr::handle)->void {
        update();
    };
    m_update_timer_handle = m_looper->create_timer(update_time, update_callback);
}

void network_client::stop() {
    // todo: sync?
    if (m_state == state::idle) {
        throw illegal_state_exception();
    }

    if (m_update_timer_handle != empty_handle) {
        m_looper->stop_timer(m_update_timer_handle);
        m_update_timer_handle = empty_handle;
    }

    // todo: could throw if already stopped
    // todo: must be done in looper thread
    m_io.stop();
    m_state = state::idle;
}

void network_client::update() {
    if (m_state == state::idle) {
        // we aren't running even
        return;
    }

    if (m_clock_sync_timer.is_running() && m_clock_sync_timer.has_elapsed(server_sync_time)) {
        TRACE_DEBUG(LOG_MODULE, "requesting time sync from server");
        const auto now = m_clock->now();
        m_message_queue.enqueue(out_message::time_sync_request(now), message_queue::flag_immediate);
        m_clock_sync_timer.stop();
    }

    switch (m_state) {
        case state::opening: {
            if (m_connect_retry_timer.is_running() && !m_connect_retry_timer.has_elapsed(connect_retry_time)) {
                break;
            }

            if (do_open_and_connect()) {
                m_connect_retry_timer.stop();
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

bool network_client::do_open_and_connect() {
    try {
        m_io.start(m_looper);
        m_io.connect(m_conn_info);
        m_state = state::connecting;

        return true;
    } catch (const std::exception& e) {
        TRACE_ERROR(LOG_MODULE, "error while opening and starting network_client: what=%s", e.what());
        m_io.stop(); // todo: could throw

        return false;
    }
}

void network_client::process_storage() {
    m_storage->act_on_dirty_entries([this](const storage::storage_entry& entry) -> bool {
        const auto id = entry.get_net_id();

        if (id == storage::id_not_assigned) {
            // entry was created
            auto value = entry.get_value();
            m_message_queue.enqueue(out_message::entry_create(
                    m_clock->now(),
                    id,
                    entry.get_path(),
                    std::move(value)
            ));

            return true;
        }

        if (entry.has_flags(storage::flag_internal_deleted)) {
            // entry was deleted
            m_message_queue.enqueue(out_message::entry_deleted(
                    entry.get_last_update_timestamp(),
                    id
            ));
        } else {
            // entry value_raw was updated
            auto value = entry.get_value();
            m_message_queue.enqueue(out_message::entry_update(
                    entry.get_last_update_timestamp(),
                    id,
                    std::move(value)
            ));
        }

        // we want to mark un-dirty and resume if we succeeded
        return true;
    });
}

}
