
#include "internal_except.h"
#include "util/general.h"
#include "util/time.h"
#include "debug.h"

#include "server.h"

namespace obsr::net::server {

#define LOG_MODULE "server"

static constexpr auto open_retry_time = std::chrono::milliseconds(1000);
static constexpr auto update_time = std::chrono::milliseconds(200);

server_client::server_client(client_id id, looper::tcp tcp, const clock_ref& clock, on_message_cb&& message_cb, on_error_cb&& error_cb)
    : m_id(id)
    , m_tcp(tcp)
    , m_clock(clock)
    , m_state(state::connected)
    , m_reader(1024)
    , m_message_cb(std::move(message_cb))
    , m_error_cb(std::move(error_cb))
    , m_published_entries()
    , m_queue()
    , m_write_buffer(1024)
    , m_writing_in_progress(false) {
    m_queue.attach([this](uint8_t type, const uint8_t* buffer, size_t size)->bool {
        if (!m_write_buffer.can_write(sizeof(message_header) + size)) {
            TRACE_DEBUG(LOG_MODULE, "write circular_buffer does not have enough space");
            return false;
        }

        message_header header {
                message_header::message_magic,
                message_header::current_version,
                0,
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
                invoke_func_nolock(m_error_cb, m_id);
                return false;
            }
        }

        return true;
    });

    auto read_callback = [this](looper::loop loop, looper::tcp tcp, std::span<const uint8_t> buffer, looper::error error)->void {
        if (error != 0) {
            invoke_func_nolock(m_error_cb, m_id);
            return;
        }

        m_reader.update(buffer);
        process_new_data();
    };
    looper::start_tcp_read(tcp, read_callback);
}

server_client::~server_client() {
    looper::destroy_tcp(m_tcp);
}

client_id server_client::get_id() const {
    return m_id;
}

server_client::state server_client::get_state() const {
    return m_state;
}

void server_client::set_state(state state) {
    m_state = state;
}

bool server_client::is_known(storage::entry_id id) const {
    return m_published_entries.find(id) != m_published_entries.end();
}

void server_client::publish(storage::entry_id id, std::string_view name) {
    TRACE_DEBUG(LOG_MODULE, "publishing entry for server client %d, entry=%d", m_id, id);
    enqueue(out_message::entry_id_assign(id, name));

    m_published_entries.insert(id);
}

void server_client::enqueue(const out_message& message, uint8_t flags) {
    TRACE_DEBUG(LOG_MODULE, "enqueuing message for server client %d", m_id);
    m_queue.enqueue(message, flags);
}

void server_client::clear() {
    m_queue.clear();
}

void server_client::update() {
    if (!m_writing_in_progress) {
        m_queue.process();

        m_writing_in_progress = true;
        looper::write_tcp(m_tcp, {m_write_buffer.data(), m_write_buffer.pos()}, [this](looper::loop loop, looper::tcp tcp, looper::error error)->void {
            if (error != 0) {
                TRACE_ERROR(LOG_MODULE, "write to tcp failed: code=%d", error);
                invoke_func_nolock(m_error_cb, m_id);
                return;
            }

            m_write_buffer.reset();
            m_writing_in_progress = false;
        });
    }
}

void server_client::process_new_data() {
    bool run;
    do {
        run = false;
        m_reader.process();

        if (m_reader.is_errored()) {
            TRACE_ERROR(LOG_MODULE, "read update error %d", m_reader.error_code());
            invoke_func_nolock(m_error_cb, m_id);
        } else if (m_reader.is_finished()) {
            auto& state = m_reader.data();
            TRACE_DEBUG(LOG_MODULE, "new message processed %d", state.header.index);

            invoke_func_nolock<client_id, const message_header&, const uint8_t*, size_t>(
                    m_message_cb,
                    m_id,
                    state.header,
                    state.message_buffer,
                    state.header.message_size);

            m_reader.reset();

            // read one message, there might be another
            run = true;
        } else {
            TRACE_DEBUG(LOG_MODULE, "message processor didn't finish, try again when more data is received");
        }
    } while (run);
}

network_server::network_server(clock_ref& clock)
    : m_mutex()
    , m_state(state::idle)
    , m_clock(clock)
    , m_storage()
    , m_bind_port(0)
    , m_loop(looper::empty_handle)
    , m_tcp(looper::empty_handle)
    , m_parser()
    , m_next_entry_id(0)
    , m_next_client_id(0)
    , m_clients()
    , m_id_assignments() {
}

void network_server::configure_bind(uint16_t bind_port) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::idle) {
        throw illegal_state_exception("server running, cannot reconfigure");
    }

    m_bind_port = bind_port;
}

void network_server::attach_storage(std::shared_ptr<storage::storage> storage) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::idle) {
        throw illegal_state_exception("server running, cannot reconfigure");
    }

    m_storage = storage;
}

void network_server::start(looper::loop loop) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::idle) {
        throw illegal_state_exception("server already running");
    }

    if (!m_storage) {
        throw illegal_state_exception("server cannot start without storage being attached");
    }

    if (m_bind_port == 0) {
        throw illegal_state_exception("server cannot start without binding being configured");
    }

    m_next_entry_id = 0;
    m_clients.clear();
    m_storage->clear_net_ids();

    m_loop = loop;

    m_state = state::opening;

    auto update_callback = [this](looper::loop loop, looper::timer timer)->void {
        update();
        looper::reset_timer(timer);
    };
    m_update_timer_handle = looper::create_timer(m_loop, update_time, update_callback);
    looper::start_timer(m_update_timer_handle);
}

void network_server::stop() {
    std::unique_lock lock(m_mutex);

    if (m_state == state::idle) {
        throw illegal_state_exception("not running");
    }

    if (m_update_timer_handle != looper::empty_handle) {
        looper::destroy_timer(m_update_timer_handle);
        m_update_timer_handle = looper::empty_handle;
    }

    close_io();

    m_state = state::idle;
}

void network_server::update() {
    if (m_state == state::idle) {
        // no clients, no need to update the information
        return;
    }

    switch (m_state) {
        case state::opening: {
            if (m_open_retry_timer.is_running() && !m_open_retry_timer.has_elapsed(open_retry_time)) {
                break;
            }

            if (do_open()) {
                m_open_retry_timer.stop();
            } else {
                m_open_retry_timer.start();
            }

            break;
        }
        case state::in_use:
            if (!m_clients.empty()) {
                process_updates();
            }
            break;
        default:
            break;
    }
}

bool network_server::do_open() {
    try {
        m_tcp = looper::create_tcp_server(m_loop);
        looper::bind_tcp_server(m_tcp, m_bind_port);
        looper::listen_tcp(m_tcp, 5, [this](looper::loop loop, looper::tcp_server server)->void {
            std::unique_lock lock(m_mutex);

            auto message_cb = [this](client_id id, const message_header& header, const uint8_t* buffer, size_t size)->void {
                std::unique_lock lock(m_mutex);
                on_new_message(id, header, buffer, size);
            };
            auto error_cb = [this](client_id id)->void {
                std::unique_lock lock(m_mutex);

                auto it = m_clients.find(id);
                if (it != m_clients.end()) {
                    m_clients.erase(it);
                }
            };

            try {
                auto tcp = looper::accept_tcp(server);

                auto id = m_next_client_id++;
                auto client_u = std::make_unique<server_client>(id, tcp, m_clock, message_cb, error_cb);
                auto [it, _] = m_clients.emplace(id, std::move(client_u));

                auto& client = it->second;
                client->set_state(server_client::state::in_handshake);
            } catch (const std::exception& e) {
                TRACE_ERROR(LOG_MODULE, "error accepting new client");
                close_io();
            }
        });

        m_state = state::in_use;

        return true;
    } catch (const std::exception& e) {
        TRACE_ERROR(LOG_MODULE, "error while opening and starting server: what=%s", e.what());
        close_io();

        return false;
    }
}

void network_server::process_updates() {
    m_storage->act_on_dirty_entries([this](const storage::storage_entry& entry) -> bool {
        auto id = entry.get_net_id();

        if (id == storage::id_not_assigned) {
            id = assign_id_to_entry(entry.get_path());
        }

        auto out_message = out_message::empty();
        if (entry.has_flags(storage::flag_internal_deleted)) {
            // entry deleted
            out_message = out_message::entry_deleted(
                    m_clock->now(),
                    id);
        } else {
            // entry updated
            auto value = entry.get_value();
            out_message = out_message::entry_update(
                    entry.get_last_update_timestamp(),
                    id,
                    std::move(value));
        }

        for (auto& [client_id, client]: m_clients) {
            if (!client->is_known(id)) {
                client->publish(id, entry.get_path());
            }

            if (out_message.type() != message_type::no_type) {
                client->enqueue(out_message);
            }
        }

        return true;
    });

    for (auto& [client_id, client] : m_clients) {
        client->update();
    }
}

void network_server::on_new_message(client_id id, const message_header& header, const uint8_t* buffer, size_t size) {
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

    TRACE_DEBUG(LOG_MODULE, "received new message from client=%d of m_type=%d", id, type);

    auto parse_data = m_parser.data();
    switch (type) {
        case message_type::entry_create: {
            if (parse_data.id == storage::id_not_assigned) {
                parse_data.id = assign_id_to_entry(parse_data.name);
            }

            auto value = obsr::value(parse_data.value);
            invoke_sharedptr_nolock<storage::storage, storage::entry_id, std::string_view, const obsr::value&, std::chrono::milliseconds>(
                    m_storage,
                    &storage::storage::on_entry_created,
                    parse_data.id,
                    parse_data.name,
                    parse_data.value,
                    parse_data.send_time);

            publish_and_update_entry_for_clients(
                    parse_data.id,
                    parse_data.name,
                    std::move(value),
                    parse_data.send_time,
                    id);
            break;
        }
        case message_type::entry_update: {
            auto value = obsr::value(parse_data.value);

            invoke_sharedptr_nolock<storage::storage, storage::entry_id, const obsr::value&, std::chrono::milliseconds>(
                    m_storage,
                    &storage::storage::on_entry_updated,
                    parse_data.id,
                    parse_data.value,
                    parse_data.send_time);

            auto message_to_others = out_message::entry_update(
                    parse_data.send_time,
                    parse_data.id,
                    std::move(value));
            enqueue_message_for_clients(message_to_others, id);
            break;
        }
        case message_type::entry_delete: {
            invoke_sharedptr_nolock<storage::storage, storage::entry_id, std::chrono::milliseconds>(
                    m_storage,
                    &storage::storage::on_entry_deleted,
                    parse_data.id,
                    parse_data.send_time);

            auto message_to_others = out_message::entry_deleted(
                    parse_data.send_time,
                    parse_data.id);
            enqueue_message_for_clients(message_to_others, id);
            break;
        }
        case message_type::time_sync_request: {
            const auto now = m_clock->now();
            enqueue_message_for_client(id,
                                       out_message::time_sync_response(
                                               now,
                                               parse_data.send_time),
                                       message_queue::flag_immediate);
            break;
        }
        case message_type::handshake_ready:
            handle_do_handshake_for_client(id);
            break;
        case message_type::entry_id_assign:
        case message_type::handshake_finished:
        case message_type::time_sync_response:
            // clients should not send this
        case message_type::no_type:
        default:
            break;
    }
}

storage::entry_id network_server::assign_id_to_entry(std::string_view name) {
    auto id = m_next_entry_id++;
    m_id_assignments.emplace(id, name);
    m_storage->on_entry_id_assigned(id, name);

    return id;
}

void network_server::enqueue_message_for_clients(const out_message& message, client_id id_to_skip) {
    for (auto& [id, client] : m_clients) {
        if (id == id_to_skip) {
            continue;
        }

        client->enqueue(message);
    }
}

void network_server::enqueue_message_for_client(client_id id, const out_message& message, uint8_t flags) {
    auto it = m_clients.find(id);
    if (it == m_clients.end()) {
        return;
    }

    it->second->enqueue(message, flags);
}

void network_server::publish_and_update_entry_for_clients(
        storage::entry_id entry_id,
        const std::string& name,
        obsr::value&& value,
        std::chrono::milliseconds value_time,
        client_id id_to_skip) {
    const auto message = out_message::entry_update(value_time, entry_id, std::move(value));
    for (auto& [id, client] : m_clients) {
        if (id == id_to_skip) {
            continue;
        }

        client->publish(id, name);
        client->enqueue(message);
    }
}

void network_server::handle_do_handshake_for_client(client_id id) {
    auto it = m_clients.find(id);
    if (it == m_clients.end()) {
        return;
    }

    auto& client = it->second;

    const auto now = m_clock->now();
    for (auto& [entry_id, name] : m_id_assignments) {
        if (client->is_known(entry_id)) {
            continue;
        }

        client->publish(entry_id, name);

        auto value_opt = m_storage->get_entry_value_from_id(entry_id);
        if (value_opt) {
            auto& value = value_opt.value();
            client->enqueue(out_message::entry_update(now, entry_id, std::move(value)));
        }
    }

    client->enqueue(out_message::handshake_finished());
    client->set_state(server_client::state::in_use);

    TRACE_INFO(LOG_MODULE, "finished writing handshake data to server client %d", client->get_id());
}

void network_server::close_io() {
    m_clients.clear();
    m_state = state::opening;

    if (m_tcp != looper::empty_handle) {
        looper::destroy_tcp_server(m_tcp);
        m_tcp = looper::empty_handle;
    }
}

}
