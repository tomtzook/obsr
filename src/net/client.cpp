
#include <mutex>

#include "debug.h"
#include "internal_except.h"
#include "util/general.h"

#include "client.h"

namespace obsr::net::client {

#define LOG_MODULE "network_client"

static constexpr auto connect_retry_time = std::chrono::milliseconds(1000);
static constexpr auto server_sync_time = std::chrono::milliseconds(1000);
static constexpr auto update_time = std::chrono::milliseconds(200);


network_client::network_client(const clock_ptr& clock)
    : m_mutex()
    , m_state(state::idle)
    , m_clock(clock)
    , m_storage(nullptr)
    , m_conn_info({"", 0})
    , m_loop(looper::empty_handle)
    , m_tcp()
    , m_update_timer_handle()
    , m_reader(1024)
    , m_parser()
    , m_message_queue(std::bind_front(&network_client::write_new_message, this))
    , m_write_buffer(1024)
    , m_next_message_id(0) {
}

void network_client::configure_target(connection_info info) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::idle) {
        throw illegal_state_exception("client running, cannot reconfigure");
    }

    m_conn_info = std::move(info);
}

void network_client::attach_storage(std::shared_ptr<storage::storage> storage) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::idle) {
        throw illegal_state_exception("client running, cannot reconfigure");
    }

    m_storage = std::move(storage);
}

void network_client::attach_diagnostics_dispatcher(diagnostics::event_dispatcher_ptr dispatcher) {
    std::unique_lock lock(m_mutex);
    m_diagnostics_dispatcher = std::move(dispatcher);
}

void network_client::start(const looper::loop loop) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::idle) {
        throw illegal_state_exception("already running");
    }

    if (!m_storage) {
        throw illegal_state_exception("cannot start without attached storage");
    }

    if (m_conn_info.port == 0) {
        throw illegal_state_exception("cannot start without target info");
    }

    m_storage->clear_net_ids();
    m_connect_retry_timer.stop();
    m_clock_sync_timer.stop();
    m_message_queue.clear();

    m_loop = loop;

    m_state = state::opening;

    auto update_callback = [this](const looper::timer timer)->void {
        std::lock_guard lock_cb(m_mutex);

        update();
        looper::reset_timer(timer);
    };
    m_update_timer_handle = looper::make_timer(m_loop, update_time, update_callback);
    looper::start_timer(m_update_timer_handle);
}

void network_client::stop() {
    std::unique_lock lock(m_mutex);

    if (m_state == state::idle) {
        throw illegal_state_exception("not running");
    }

    m_update_timer_handle.reset();
    close_io();

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
        enqueue_message(out_message::time_sync_request(now), message_queue::flag_immediate);
        m_clock_sync_timer.stop();
    }

    bool process_out_queue = false;

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
            process_out_queue = true;
            break;
        case state::in_handshake_time_sync:
        case state::in_handshake:
            // in this phase we do not send anything to the server, just receive data
            process_out_queue = true;
            break;
        case state::connecting:
        case state::idle:
        default:
            break;
    }

    if (process_out_queue) {
        m_message_queue.process();
    }
}

bool network_client::do_open_and_connect() {
    try {
        m_tcp = looper::create_tcp(m_loop);
        looper::connect_tcp(m_tcp, m_conn_info.ip, m_conn_info.port, [this](looper::tcp, const looper::error error)->void {
            std::unique_lock lock_cb1(m_mutex);

            if (error != 0) {
                TRACE_ERROR(LOG_MODULE, "error while connecting client: code=%d", error);
                close_io();
                return;
            }

            TRACE_DEBUG(LOG_MODULE, "connected to server, starting first time sync");
            m_message_queue.clear();

            const auto now = m_clock->now();
            enqueue_message(out_message::time_sync_request(now), message_queue::flag_immediate);
            m_state = state::in_handshake_time_sync;

            auto read_callback = [this](looper::tcp, const std::span<const uint8_t> buffer, const looper::error error)->void {
                std::unique_lock lock_cb2(m_mutex);

                if (error != 0) {
                    TRACE_ERROR(LOG_MODULE, "error while reading client: code=%d", error);
                    close_io();
                    return;
                }

                m_reader.update(buffer);
                process_new_data();
            };
            looper::start_tcp_read(m_tcp, read_callback);
        });

        m_state = state::connecting;

        return true;
    } catch (const std::exception& e) {
        TRACE_ERROR(LOG_MODULE, "error while opening and starting client: what=%s", e.what());
        close_io();

        return false;
    }
}

void network_client::process_storage() {
    m_storage->act_on_dirty_entries([this](const storage::storage_entry& entry) -> bool {
        const auto id = entry.get_net_id();

        if (id == storage::id_not_assigned) {
            // entry was created
            auto value = entry.get_value();
            enqueue_message(out_message::entry_create(
                    m_clock->now(),
                    entry.get_path(),
                    std::move(value)
            ));
        } else if (entry.has_flags(storage::flag_internal_deleted)) {
            // entry was deleted
            enqueue_message(out_message::entry_deleted(
                    entry.get_last_update_timestamp(),
                    id
            ));
        } else {
            // entry value was updated
            auto value = entry.get_value();
            enqueue_message(out_message::entry_update(
                    entry.get_last_update_timestamp(),
                    id,
                    std::move(value)
            ));
        }

        // we want to mark un-dirty and resume if we succeeded
        return true;
    });
}

void network_client::process_new_data() {
    bool run;
    do {
        run = false;
        m_reader.process();

        if (m_reader.is_errored()) {
            TRACE_ERROR(LOG_MODULE, "read update error %d", m_reader.error_code());
            close_io();
        } else if (m_reader.is_finished()) {
            auto& state = m_reader.data();
            TRACE_DEBUG(LOG_MODULE, "new message processed %d", state.header.index);

            on_new_message(state.header, state.message_buffer, state.header.message_size);

            m_reader.reset();

            // read one message, there might be another
            run = true;
        } else {
            TRACE_DEBUG(LOG_MODULE, "message processor didn't finish, try again when more data is received");
        }
    } while (run);
}

void network_client::on_new_message(const message_header& header, const uint8_t* buffer, const size_t size) {
    const auto type = static_cast<message_type>(header.type);
    m_parser.set_data(type, buffer, size);
    m_parser.process();

    if (m_parser.is_errored()) {
        TRACE_ERROR(LOG_MODULE, "failed to parse incoming data, parser error=%d", m_parser.error_code());
        return;
    }
    if (!m_parser.is_finished()) {
        TRACE_ERROR(LOG_MODULE, "failed to parse incoming data, parser did not finish");
        return;
    }

    const auto parse_data = m_parser.data();
    diagnostics::notify_received_message(m_diagnostics_dispatcher, type, 0, header.index);

    switch (type) {
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
                enqueue_message(out_message::handshake_ready());
                m_state = state::in_handshake;
            } else {
                m_clock_sync_timer.start();
            }
            break;
        }
        case message_type::entry_create:
        case message_type::no_type:
        default:
            break;
    }
}

void network_client::enqueue_message(out_message&& message, const uint8_t flags) {
    const auto message_id = ++m_next_message_id;
    m_message_queue.enqueue(std::move(message), message_id, flags);
    diagnostics::notify_sending_message(m_diagnostics_dispatcher, message.type(), 0, message_id);
}

bool network_client::write_new_message(const uint8_t type, const uint8_t* buffer, const size_t size,
    const client_id, const client_id, const uint64_t message_id) {
    if (!m_write_buffer.can_write(sizeof(message_header) + size)) {
        TRACE_DEBUG(LOG_MODULE, "write circular_buffer does not have enough space");
        return false;
    }

    message_header header {
            message_header::message_magic,
            message_header::current_version,
            message_id,
            type,
            static_cast<uint32_t>(size)
    };
    header_convert_net(header);

    if (!m_write_buffer.write(reinterpret_cast<uint8_t*>(&header), sizeof(header))) {
        TRACE_DEBUG(LOG_MODULE, "write failed to buffer at start");
        return false;
    }

    if (buffer != nullptr && size > 0) {
        if (!m_write_buffer.write(buffer, size)) {
            // this means we have probably sent a message with a header but no data. this will seriously
            // break down communication. as such, we will terminate connection here.
            TRACE_ERROR(LOG_MODULE, "write attempt failed halfway, stopping");
            close_io();
            return false;
        }
    }

    looper::write_tcp(m_tcp, {m_write_buffer.data(), m_write_buffer.pos()}, [this](looper::tcp, const looper::error error)->void {
        if (error != 0) {
            TRACE_ERROR(LOG_MODULE, "write to tcp failed: code=%d", error);
            close_io();
            return;
        }
    });

    m_write_buffer.reset();

    return true;
}

void network_client::close_io() {
    m_update_timer_handle.reset();
    m_tcp.reset();

    m_connect_retry_timer.stop();
    m_clock_sync_timer.stop();

    m_state = state::opening;
    m_connect_retry_timer.start();
}

}
