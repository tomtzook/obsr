#pragma once

#include <set>
#include <deque>

#include "storage/storage.h"
#include "net/io.h"
#include "net/serialize.h"
#include "net/net.h"

namespace obsr::net {

struct server_client : public message_queue::destination {
public:
    enum class state {
        connected,
        in_handshake,
        in_use,
    };

    server_client(server_io::client_id id, server_io& io, const std::shared_ptr<clock>& clock);

    server_io::client_id get_id() const;

    state get_state() const;
    void set_state(state state);

    bool is_known(storage::entry_id id) const;

    void publish(storage::entry_id id, std::string_view name);

    void enqueue(const out_message& message);
    void clear();

    void update();

    std::chrono::milliseconds get_time_now() override;
    bool write(uint8_t type, const uint8_t* buffer, size_t size) override;

private:
    server_io::client_id m_id;
    server_io& m_io;
    std::shared_ptr<clock> m_clock;
    state m_state;

    message_queue m_queue;
    std::set<storage::entry_id> m_published_entries;
};

class server : public server_io::listener, public network_interface {
public:
    explicit server(const std::shared_ptr<io::nio_runner>& nio_runner, const std::shared_ptr<clock>& clock);

    void attach_storage(std::shared_ptr<storage::storage> storage) override;

    void start(uint16_t bind_port);
    void stop() override;

    void update() override;

    // events
    void on_client_connected(server_io::client_id id) override;
    void on_client_disconnected(server_io::client_id id) override;
    void on_new_message(server_io::client_id id, const message_header& header, const uint8_t* buffer, size_t size) override;
    void on_close() override;

private:
    storage::entry_id assign_id_to_entry(std::string_view name);
    void enqueue_message_for_clients(const out_message& message, server_io::client_id id_to_skip = static_cast<server_io::client_id>(-1));
    void enqueue_message_for_client(server_io::client_id id, const out_message& message);

    void handle_do_handshake_for_client(server_io::client_id id);

    std::shared_ptr<clock> m_clock;

    std::mutex m_mutex;

    server_io m_io;
    message_parser m_parser;
    std::shared_ptr<storage::storage> m_storage;

    storage::entry_id m_next_entry_id;
    std::map<server_io::client_id, std::unique_ptr<server_client>> m_clients;
    std::map<storage::entry_id, std::string> m_id_assignments;
};

}
