#pragma once

#include <set>
#include <deque>

#include <looper.h>
#include <looper_tcp.h>

#include "storage/storage.h"
#include "net/io.h"
#include "net/serialize.h"
#include "net/net.h"

namespace obsr::net::server {

using client_id = uint16_t;
static constexpr client_id invalid_client_id = static_cast<client_id>(-1);

class server_client {
public:
    using on_error_cb = std::function<void(client_id)>;
    using on_message_cb = std::function<void(client_id, const message_header&, const uint8_t*, size_t)>;

    enum class state {
        connected,
        in_handshake,
        in_use,
    };

    server_client(client_id id, looper::tcp tcp, const clock_ref& clock, on_message_cb&& message_cb, on_error_cb&& error_cb);
    ~server_client();

    client_id get_id() const;

    state get_state() const;
    void set_state(state state);

    bool is_known(storage::entry_id id) const;

    void publish(storage::entry_id id, std::string_view name);

    void enqueue(const out_message& message, uint8_t flags = 0);
    void clear();

    void update();

private:
    void process_new_data();

    client_id m_id;
    looper::tcp m_tcp;
    clock_ref m_clock;
    state m_state;

    reader m_reader;
    on_message_cb m_message_cb;
    on_error_cb m_error_cb;

    io::linear_buffer m_write_buffer;
    message_queue m_queue;
    std::set<storage::entry_id> m_published_entries;
};

class network_server : public network_interface {
public:
    explicit network_server(clock_ref& clock);

    void configure_bind(uint16_t bind_port);

    void attach_storage(std::shared_ptr<storage::storage> storage) override;
    void start(looper::loop loop) override;
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
    void on_new_message(client_id id, const message_header& header, const uint8_t* buffer, size_t size);

    storage::entry_id assign_id_to_entry(std::string_view name);
    void enqueue_message_for_clients(const out_message& message, client_id id_to_skip = invalid_client_id);
    void enqueue_message_for_client(client_id id, const out_message& message, uint8_t flags = 0);
    void publish_and_update_entry_for_clients(
            storage::entry_id entry_id,
            const std::string& name,
            obsr::value&& value,
            std::chrono::milliseconds value_time,
            client_id id_to_skip = invalid_client_id);

    void handle_do_handshake_for_client(client_id id);
    void close_io();

    std::mutex m_mutex;
    state m_state;

    clock_ref m_clock;
    std::shared_ptr<storage::storage> m_storage;
    uint16_t m_bind_port;

    looper::loop m_loop;

    looper::tcp_server m_tcp;
    message_parser m_parser;

    client_id m_next_client_id;
    storage::entry_id m_next_entry_id;
    std::map<client_id, std::unique_ptr<server_client>> m_clients;
    std::map<storage::entry_id, std::string> m_id_assignments;

    looper::timer m_update_timer_handle;
    timer m_open_retry_timer;
};

}
