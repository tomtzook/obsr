#pragma once

#include <set>
#include <deque>

#include "storage/storage.h"
#include "net/io.h"
#include "net/serialize.h"

namespace obsr::net {

struct out_message {
    message_type type;

    storage::entry_id id;
    std::string name;
    value_t value;
};

struct server_client {
public:
    enum class state {
        connected,
        in_handshake,
        in_run,
        disconnected
    };

    server_client();

    bool is_known(storage::entry_id id) const;

    void publish(storage::entry_id id, std::string_view name);
    void enqueue(const out_message& message);
    void process();

private:
    bool write_message(const out_message& message);
    bool write_entry_created(const out_message& message);
    bool write_entry_updated(const out_message& message);
    bool write_entry_deleted(const out_message& message);
    bool write_entry_id_assigned(const out_message& message);

    server_io::client_id m_id;
    server_io& m_io;

    state m_state;

    message_writer m_writer;
    std::set<storage::entry_id> m_published_entries;
    std::map<storage::entry_id, std::deque<out_message>> m_outgoing;
};

class server : public server_io::listener {
public:
    server(std::shared_ptr<io::nio_runner> nio_runner);

    void attach_storage(std::shared_ptr<storage::storage> storage);

    void start(int bind_port);
    void stop();

    void process();

    // events
    void on_client_connected(server_io::client_id id) override;
    void on_client_disconnected(server_io::client_id id) override;
    void on_new_message(server_io::client_id id, const message_header& header, const uint8_t* buffer, size_t size) override;
    void on_close() override;

private:
    storage::entry_id assign_id_to_entry(std::string_view name);
    void enqueue_message_for_clients(const out_message& message, server_io::client_id id_to_skip = static_cast<server_io::client_id>(-1));

    std::mutex m_mutex;

    server_io m_io;
    message_parser m_parser;
    std::shared_ptr<storage::storage> m_storage;

    storage::entry_id m_next_entry_id;
    std::map<server_io::client_id, server_client> m_clients;
    std::map<storage::entry_id, std::string> m_id_assignments;
};

}
