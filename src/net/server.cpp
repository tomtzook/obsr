
#include "internal_except.h"
#include "util/general.h"
#include "util/time.h"
#include "debug.h"

#include "server.h"

namespace obsr::net {

#define LOG_MODULE "server"

server_client::server_client(server_io::client_id id, server_io& io, const std::shared_ptr<clock>& clock)
    : m_id(id)
    , m_io(io)
    , m_clock(clock)
    , m_state(state::connected)
    , m_queue(this)
    , m_published_entries()
{}

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

bool server_client::write(uint8_t type, const uint8_t* buffer, size_t size) {
    return m_io.write_to(m_id, type, buffer, size);
}

server::server(const std::shared_ptr<io::nio_runner>& nio_runner, const std::shared_ptr<clock>& clock)
    : m_clock(clock)
    , m_mutex()
    , m_io(nio_runner, this)
    , m_parser()
    , m_storage()
    , m_next_entry_id(0)
    , m_clients()
{}

void server::attach_storage(std::shared_ptr<storage::storage> storage) {
    m_storage = std::move(storage);
}

void server::start(uint16_t bind_port) {
    std::unique_lock lock(m_mutex);

    if (!m_storage) {
        throw illegal_state_exception();
    }

    m_next_entry_id = 0;
    m_clients.clear();
    m_storage->clear_net_ids();

    m_io.start(bind_port);
}

void server::stop() {
    std::unique_lock lock(m_mutex);

    m_io.stop();
}

void server::update() {
    std::unique_lock lock(m_mutex);

    if (m_io.is_stopped()) {
        // not running even
        return;
    }

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

void server::on_client_connected(server_io::client_id id) {
    std::unique_lock lock(m_mutex);

    auto client_u = std::make_unique<server_client>(id, m_io, m_clock);
    auto [it, _] = m_clients.emplace(id, std::move(client_u));

    auto& client = it->second;
    client->set_state(server_client::state::in_handshake);
}

void server::on_client_disconnected(server_io::client_id id) {
    std::unique_lock lock(m_mutex);

    auto it = m_clients.find(id);
    if (it != m_clients.end()) {
        m_clients.erase(it);
    }
}

void server::on_new_message(server_io::client_id id, const message_header& header, const uint8_t* buffer, size_t size) {
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

    TRACE_DEBUG(LOG_MODULE, "received new message from client=%d of m_type=%d", id, type);

    auto message_to_others = out_message::empty();

    auto parse_data = m_parser.data();
    switch (type) {
        case message_type::entry_create:
            if (parse_data.id == storage::id_not_assigned) {
                parse_data.id = assign_id_to_entry(parse_data.name);
            }

            message_to_others = out_message::entry_create(parse_data.time,
                                                          parse_data.id,
                                                          parse_data.name,
                                                          obsr::value(parse_data.value));
            invoke_shared_ptr<storage::storage, storage::entry_id, std::string_view, const obsr::value&, std::chrono::milliseconds>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_created,
                    parse_data.id,
                    parse_data.name,
                    parse_data.value,
                    parse_data.time);
            break;
        case message_type::entry_update:
            message_to_others = out_message::entry_update(parse_data.time,
                                                          parse_data.id,
                                                          obsr::value(parse_data.value));
            invoke_shared_ptr<storage::storage, storage::entry_id, const obsr::value&, std::chrono::milliseconds>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_updated,
                    parse_data.id,
                    parse_data.value,
                    parse_data.time);
            break;
        case message_type::entry_delete:
            message_to_others = out_message::entry_deleted(parse_data.time, parse_data.id);
            invoke_shared_ptr<storage::storage, storage::entry_id, std::chrono::milliseconds>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_deleted,
                    parse_data.id,
                    parse_data.time);
            break;
        case message_type::time_sync_request: {
            const auto now = m_clock->now();
            enqueue_message_for_client(id,
                                       out_message::time_sync_response(now, parse_data.time),
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
}

void server::on_close() {

}

storage::entry_id server::assign_id_to_entry(std::string_view name) {
    auto id = m_next_entry_id++;
    m_id_assignments.emplace(id, name);
    m_storage->on_entry_id_assigned(id, name);

    return id;
}

void server::enqueue_message_for_clients(const out_message& message, server_io::client_id id_to_skip) {
    for (auto& [id, client] : m_clients) {
        if (id == id_to_skip) {
            continue;
        }

        client->enqueue(message);
    }
}

void server::enqueue_message_for_client(server_io::client_id id, const out_message& message, uint8_t flags) {
    auto it = m_clients.find(id);
    if (it == m_clients.end()) {
        return;
    }

    it->second->enqueue(message, flags);
}

void server::handle_do_handshake_for_client(server_io::client_id id) {
    auto it = m_clients.find(id);
    if (it == m_clients.end()) {
        return;
    }

    auto& client = it->second;

    const auto now = m_clock->now();
    //todo: stop using value_raw, switch to value
    //  find a way to make sure we don't create too many copies of the same value
    //  will be problematic for values with allocated buffers (in the future)
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
