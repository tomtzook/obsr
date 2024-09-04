
#include "internal_except.h"
#include "util/general.h"
#include "util/time.h"
#include "debug.h"

#include "server.h"

namespace obsr::net {

#define LOG_MODULE "server"

static constexpr auto update_time = std::chrono::milliseconds(200);

server_client::server_client(server_io::client_id id, server_io& parent, const std::shared_ptr<clock>& clock)
    : m_id(id)
    , m_parent(parent)
    , m_clock(clock)
    , m_state(state::connected)
    , m_published_entries()
    , m_queue() {
    m_queue.attach([this](uint8_t type, const uint8_t* buffer, size_t size)->bool {
        return m_parent.write_to(m_id, type, buffer, size);
    });
}

server_io::client_id server_client::get_id() const {
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
    m_queue.process();
}

network_server::network_server(std::shared_ptr<clock>& clock)
    : m_mutex()
    , m_looper()
    , m_looper_thread()
    , m_clock(clock)
    , m_storage()
    , m_bind_port(0)
    , m_update_timer_handle(empty_handle)
    , m_io()
    , m_parser()
    , m_next_entry_id(0)
    , m_clients()
    , m_id_assignments() {
    m_io.on_connect([this](server_io::client_id id)->void {
        auto client_u = std::make_unique<server_client>(id, m_io, m_clock);
        auto [it, _] = m_clients.emplace(id, std::move(client_u));

        auto& client = it->second;
        client->set_state(server_client::state::in_handshake);
    });
    m_io.on_disconnect([this](server_io::client_id id)->void {
        auto it = m_clients.find(id);
        if (it != m_clients.end()) {
            m_clients.erase(it);
        }
    });
    m_io.on_close([this]()->void {

    });
    m_io.on_message([this](server_io::client_id id, const message_header& header, const uint8_t* buffer, size_t size)->void {
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

        auto message_to_others = out_message::empty();

        auto parse_data = m_parser.data();
        switch (type) {
            case message_type::entry_create:
                if (parse_data.id == storage::id_not_assigned) {
                    parse_data.id = assign_id_to_entry(parse_data.name);
                }

                // todo: don't send create to clients, send assign and update?
                //  if that is the case, exclude id from create since only clients report it without id
                message_to_others = out_message::entry_create(parse_data.send_time,
                                                              parse_data.id,
                                                              parse_data.name,
                                                              obsr::value(parse_data.value));
                invoke_sharedptr_nolock<storage::storage, storage::entry_id, std::string_view, const obsr::value&, std::chrono::milliseconds>(
                        m_storage,
                        &storage::storage::on_entry_created,
                        parse_data.id,
                        parse_data.name,
                        parse_data.value,
                        parse_data.send_time);
                break;
            case message_type::entry_update:
                message_to_others = out_message::entry_update(parse_data.send_time,
                                                              parse_data.id,
                                                              obsr::value(parse_data.value));
                invoke_sharedptr_nolock<storage::storage, storage::entry_id, const obsr::value&, std::chrono::milliseconds>(
                        m_storage,
                        &storage::storage::on_entry_updated,
                        parse_data.id,
                        parse_data.value,
                        parse_data.send_time);
                break;
            case message_type::entry_delete:
                message_to_others = out_message::entry_deleted(parse_data.send_time, parse_data.id);
                invoke_sharedptr_nolock<storage::storage, storage::entry_id, std::chrono::milliseconds>(
                        m_storage,
                        &storage::storage::on_entry_deleted,
                        parse_data.id,
                        parse_data.send_time);
                break;
            case message_type::time_sync_request: {
                const auto now = m_clock->now();
                enqueue_message_for_client(id,
                                           out_message::time_sync_response(now, parse_data.send_time),
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
                message_to_others = out_message::empty();
                break;
        }

        if (message_to_others.type() != message_type::no_type) {
            enqueue_message_for_clients(message_to_others, id);
        }
    });
}

void network_server::configure_bind(uint16_t bind_port) {
    // todo: throw if idle
    m_bind_port = bind_port;
}

void network_server::attach_storage(std::shared_ptr<storage::storage> storage) {
    // todo: throw if idle
    m_storage = storage;
}

void network_server::start() {
    if (!m_storage) {
        throw illegal_state_exception();
    }

    if (m_bind_port == 0) {
        throw illegal_state_exception();
    }

    m_next_entry_id = 0;
    m_clients.clear();
    m_storage->clear_net_ids();

    m_looper = std::make_shared<events::looper>();
    m_looper_thread = std::make_unique<events::looper_thread>(m_looper);

    m_looper->request_execute([this](events::looper&)->void {
        m_io.start(m_looper.get(), m_bind_port);
        // todo: retry?
    });

    auto update_callback = [this](events::looper&, obsr::handle)->void {
        update();
    };
    m_update_timer_handle = m_looper->create_timer(update_time, update_callback);
}

void network_server::stop() {
    m_looper->request_execute([this](events::looper&)->void {
        if (m_update_timer_handle != empty_handle) {
            m_looper->stop_timer(m_update_timer_handle);
            m_update_timer_handle = empty_handle;
        }

        // todo: could throw if already stopped
        m_io.stop();
    }, events::looper::execute_type::sync);

    // will stop thread
    m_looper_thread.reset();
    m_looper.reset();
}

void network_server::update() {
    if (m_clients.empty()) {
        // no clients, no need to update the information
        return;
    }

    m_storage->act_on_dirty_entries([this](const storage::storage_entry& entry) -> bool {
        // todo: moving to a "push" methodology will solve this better
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

storage::entry_id network_server::assign_id_to_entry(std::string_view name) {
    auto id = m_next_entry_id++;
    m_id_assignments.emplace(id, name);
    m_storage->on_entry_id_assigned(id, name);

    return id;
}

void network_server::enqueue_message_for_clients(const out_message& message, server_io::client_id id_to_skip) {
    for (auto& [id, client] : m_clients) {
        if (id == id_to_skip) {
            continue;
        }

        client->enqueue(message);
    }
}

void network_server::enqueue_message_for_client(server_io::client_id id, const out_message& message, uint8_t flags) {
    auto it = m_clients.find(id);
    if (it == m_clients.end()) {
        return;
    }

    it->second->enqueue(message, flags);
}

void network_server::handle_do_handshake_for_client(server_io::client_id id) {
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

}
