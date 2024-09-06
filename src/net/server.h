#pragma once

#include <set>
#include <deque>

#include "storage/storage.h"
#include "net/io.h"
#include "net/serialize.h"
#include "net/net.h"
#include "events/events.h"

namespace obsr::net {

class server_client {
public:
    enum class state {
        connected,
        in_handshake,
        in_use,
    };

    server_client(server_io::client_id id, server_io& parent, const std::shared_ptr<clock>& clock);

    server_io::client_id get_id() const;

    state get_state() const;
    void set_state(state state);

    bool is_known(storage::entry_id id) const;

    void publish(storage::entry_id id, std::string_view name);

    void enqueue(const out_message& message, uint8_t flags = 0);
    void clear();

    void update();

private:
    server_io::client_id m_id;
    server_io& m_parent;
    std::shared_ptr<clock> m_clock;
    state m_state;

    message_queue m_queue;
    std::set<storage::entry_id> m_published_entries;
};

class network_server : public network_interface {
public:
    explicit network_server(std::shared_ptr<clock>& clock);

    void configure_bind(uint16_t bind_port);

    void attach_storage(std::shared_ptr<storage::storage> storage) override;
    void start(events::looper* looper) override;
    void stop() override;

private:
    enum class state {
        idle,
        opening,
        in_use
    };

    void update();
    bool do_open();
    void process_updates();

    storage::entry_id assign_id_to_entry(std::string_view name);
    void enqueue_message_for_clients(const out_message& message, server_io::client_id id_to_skip = server_io::invalid_client_id);
    void enqueue_message_for_client(server_io::client_id id, const out_message& message, uint8_t flags = 0);

    void handle_do_handshake_for_client(server_io::client_id id);

    std::mutex m_mutex;
    state m_state;

    std::shared_ptr<clock> m_clock;
    std::shared_ptr<storage::storage> m_storage;
    uint16_t m_bind_port;

    events::looper* m_looper;
    obsr::handle m_update_timer_handle;

    server_io m_io;
    message_parser m_parser;

    storage::entry_id m_next_entry_id;
    std::map<server_io::client_id, std::unique_ptr<server_client>> m_clients;
    std::map<storage::entry_id, std::string> m_id_assignments;

    timer m_open_retry_timer;
};

}
