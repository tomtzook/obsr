
#include "internal_except.h"
#include "util/general.h"
#include "util/time.h"
#include "debug.h"

#include "server.h"

namespace obsr::net {

#define LOG_MODULE "server"

server_client::server_client(server_io::client_id id, server_io& io)
    : m_id(id)
    , m_io(io)
    , m_state(state::connected)
    , m_writer()
    , m_outgoing()
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
    out_message message {
        .type = message_type::entry_id_assign,
        .id = id,
        .name = std::string(name),
    };
    enqueue(message);

    m_published_entries.insert(id);
}

void server_client::enqueue(const out_message& message) {
    TRACE_DEBUG(LOG_MODULE, "enqueuing message for server client %d, count=%d", m_id, m_outgoing.size() + 1);
    m_outgoing.push_back(message);
}

void server_client::update() {
    auto it = m_outgoing.begin();
    while (it != m_outgoing.end()) {
        const auto success = write_message(*it);
        if (success) {
            it = m_outgoing.erase(it);
        } else {
            break;
        }
    }
}

bool server_client::write_message(const out_message& message) {
    switch (message.type) {
        case message_type::entry_create:
            return write_entry_created(message);
        case message_type::entry_update:
            return write_entry_updated(message);
        case message_type::entry_delete:
            return write_entry_deleted(message);
        case message_type::entry_id_assign:
            return write_entry_id_assigned(message);
        case message_type::handshake_finished:
            return write_basic(message);
        case message_type::no_type:
        default:
            return true;
    }
}

bool server_client::write_entry_created(const out_message& message) {
    m_writer.reset();

    if (!m_writer.entry_created(message.id, message.name, message.value)) {
        return false;
    }

    if (!m_io.write_to(
            m_id,
            static_cast<uint8_t>(message_type::entry_create),
            m_writer.data(),
            m_writer.size())) {
        return false;
    }

    return true;
}

bool server_client::write_entry_updated(const out_message& message) {
    m_writer.reset();

    if (!m_writer.entry_updated(message.id, message.value)) {
        return false;
    }

    if (!m_io.write_to(
            m_id,
            static_cast<uint8_t>(message_type::entry_update),
            m_writer.data(),
            m_writer.size())) {
        return false;
    }

    return true;
}

bool server_client::write_entry_deleted(const out_message& message) {
    m_writer.reset();

    if (!m_writer.entry_deleted(message.id)) {
        return false;
    }

    if (!m_io.write_to(
            m_id,
            static_cast<uint8_t>(message_type::entry_delete),
            m_writer.data(),
            m_writer.size())) {
        return false;
    }

    return true;
}

bool server_client::write_entry_id_assigned(const out_message& message) {
    m_writer.reset();

    if (!m_writer.entry_id_assign(message.id, message.name)) {
        return false;
    }

    if (!m_io.write_to(
            m_id,
            static_cast<uint8_t>(message_type::entry_id_assign),
            m_writer.data(),
            m_writer.size())) {
        return false;
    }

    return true;
}

bool server_client::write_basic(const out_message& message) {
    if (!m_io.write_to(
            m_id,
            static_cast<uint8_t>(message.type),
            nullptr,
            0)) {
        return false;
    }

    return true;
}


server::server(const std::shared_ptr<io::nio_runner>& nio_runner)
    : m_mutex()
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
    // todo: we will not be able to send all entry changes to all clients
    //      need to save for later and iterate
    //      there could also be starvation if there are too many entries
    //  todo: moving to a "push" methodology will solve this better

    // todo: this iteration does accesses that are not safe!

    if (m_io.is_stopped()) {
        // not running even
        return;
    }

    if (m_clients.empty()) {
        // no clients, no need to update the information
        return;
    }

    m_storage->act_on_entries([this](const storage::storage_entry& entry)->bool {
        auto id = entry.get_net_id();
        auto path = entry.get_path();

        if (id == storage::id_not_assigned) {
            id = assign_id_to_entry(path);
        }

        auto dirty = entry.is_dirty();

        out_message out_message{.type = message_type::no_type};
        if (dirty) {
            if (entry.has_flags(storage::flag_internal_deleted)) {
                // entry deleted
                out_message.id = id;
                out_message.type = message_type::entry_delete;
            } else {
                // entry updated
                out_message.id = id;
                out_message.type = message_type::entry_update;
                entry.get_value(out_message.value);
            }
        }

        for (auto& [client_id, client] : m_clients) {
            if (!client->is_known(id)) {
                client->publish(id, path);
            }

            if (out_message.type != message_type::no_type) {
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

    auto client_u = std::make_unique<server_client>(id, m_io);
    auto [it, _] = m_clients.emplace(id, std::move(client_u));

    auto& client = it->second;
    client->set_state(server_client::state::in_handshake);

    handle_do_handshake_for_client(client.get());
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

    TRACE_DEBUG(LOG_MODULE, "received new message from client=%d of type=%d", id, type);

    out_message message_to_others;

    auto parse_data = m_parser.data();
    switch (type) {
        case message_type::entry_create:
            if (parse_data.id == storage::id_not_assigned) {
                parse_data.id = assign_id_to_entry(parse_data.name);
            }

            message_to_others.id = parse_data.id;
            message_to_others.name = parse_data.name;
            message_to_others.value = parse_data.value;
            message_to_others.type = message_type::entry_create;

            invoke_shared_ptr<storage::storage, storage::entry_id, std::string_view, const value&>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_created,
                    parse_data.id,
                    parse_data.name,
                    parse_data.value);
            break;
        case message_type::entry_update:
            message_to_others.id = parse_data.id;
            message_to_others.value = parse_data.value;
            message_to_others.type = message_type::entry_update;

            invoke_shared_ptr<storage::storage, storage::entry_id, const value&>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_updated,
                    parse_data.id,
                    parse_data.value);
            break;
        case message_type::entry_delete:
            message_to_others.id = parse_data.id;
            message_to_others.type = message_type::entry_delete;

            invoke_shared_ptr<storage::storage, storage::entry_id>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_deleted,
                    parse_data.id);
            break;
        case message_type::entry_id_assign:
        case message_type::handshake_finished:
            // clients should not send this
        case message_type::no_type:
        default:
            message_to_others.type = message_type::no_type;
            break;
    }

    if (message_to_others.type != message_type::no_type) {
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

void server::handle_do_handshake_for_client(server_client* client) {
    for (auto& [entry_id, name] : m_id_assignments) {
        if (client->is_known(entry_id)) {
            continue;
        }

        client->publish(entry_id, name);
    }

    client->enqueue({.type = message_type::handshake_finished});
    client->set_state(server_client::state::in_use);

    TRACE_INFO(LOG_MODULE, "finished writing handshake data to server client %d", client->get_id());
}

}
