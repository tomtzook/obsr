#pragma once

#include <set>
#include <deque>

#include "storage/storage.h"
#include "net/io.h"
#include "net/serialize.h"
#include "updater.h"

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
        in_use,
    };

    server_client(server_io::client_id id, server_io& io);
    ~server_client();

    state get_state() const;
    void set_state(state state);

    bool is_known(storage::entry_id id) const;

    void publish(storage::entry_id id, std::string_view name);
    void enqueue(const out_message& message);
    void update();

private:
    bool write_message(const out_message& message);
    bool write_entry_created(const out_message& message);
    bool write_entry_updated(const out_message& message);
    bool write_entry_deleted(const out_message& message);
    bool write_entry_id_assigned(const out_message& message);
    bool write_basic(const out_message& message);

    server_io::client_id m_id;
    server_io& m_io;
    state m_state;

    message_writer m_writer;
    std::set<storage::entry_id> m_published_entries;
    std::deque<out_message> m_outgoing;
};

class server : public server_io::listener, public updatable {
public:
    server(const std::shared_ptr<io::nio_runner>& nio_runner);

    void attach_storage(std::shared_ptr<storage::storage> storage);

    void start(int bind_port);
    void stop();

    void update() override;

    // events
    void on_client_connected(server_io::client_id id) override;
    void on_client_disconnected(server_io::client_id id) override;
    void on_new_message(server_io::client_id id, const message_header& header, const uint8_t* buffer, size_t size) override;
    void on_close() override;

private:
    storage::entry_id assign_id_to_entry(std::string_view name);
    void enqueue_message_for_clients(const out_message& message, server_io::client_id id_to_skip = static_cast<server_io::client_id>(-1));

    void handle_do_handshake_for_client(server_client* client);

    std::mutex m_mutex;

    server_io m_io;
    message_parser m_parser;
    std::shared_ptr<storage::storage> m_storage;

    storage::entry_id m_next_entry_id;
    std::map<server_io::client_id, std::unique_ptr<server_client>> m_clients;
    std::map<storage::entry_id, std::string> m_id_assignments;
};

}
