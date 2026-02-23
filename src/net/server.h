#pragma once

#include <set>

#include <looper.h>

#include "looper_cxx.hpp"
#include "obsr_internal.h"
#include "storage/storage.h"
#include "net/io.h"
#include "net/serialize.h"

namespace obsr::net::server {

class server_client {
public:
    using on_error_cb = std::function<void(client_id)>;
    using on_message_cb = std::function<void(client_id, const message_header&, const uint8_t*, size_t)>;
    using enqueue_message_cb = std::function<void(const out_message&, uint8_t)>;

    enum class state {
        connected,
        in_handshake,
        in_use,
    };

    server_client(client_id id, looper::tcp tcp, const clock_ptr& clock,
        on_message_cb&& on_message_cb, on_error_cb&& error_cb);

    client_id get_id() const;

    state get_state() const;
    void set_state(state state);

    bool is_known(storage::entry_id id) const;
    void marked_published(storage::entry_id id, std::string_view name);

    bool write_message(uint8_t type, const uint8_t* buffer, size_t size, uint64_t message_id);

private:
    void process_new_data();

    client_id m_id;
    looper::tcp_holder m_tcp;
    clock_ptr m_clock;
    state m_state;

    reader m_reader;
    io::linear_buffer m_write_buffer;
    on_message_cb m_on_message_cb;
    on_error_cb m_error_cb;
    enqueue_message_cb m_enqueue_message_cb;

    std::set<storage::entry_id> m_published_entries;
};

class network_server final {
public:
    explicit network_server(const clock_ptr& clock);

    void configure_bind(uint16_t bind_port);

    void attach_storage(const std::shared_ptr<storage::storage>& storage);
    void attach_diagnostics_dispatcher(diagnostics::event_dispatcher_ptr dispatcher);
    void start(looper::loop loop);
    void stop();

private:
    enum class state {
        idle,
        opening,
        in_use
    };

    void update();
    bool do_open();
    void process_updates();

    bool write_message_to_clients(uint8_t type, const uint8_t* buffer, size_t size, client_id destination, client_id source, uint64_t message_id);
    void on_new_message(client_id id, const message_header& header, const uint8_t* buffer, size_t size);

    storage::entry_id assign_id_to_entry(std::string_view name);

    void publish_entry_for_clients(
            storage::entry_id entry_id,
            std::string_view name,
            obsr::value&& value,
            std::chrono::milliseconds value_time,
            client_id source_id = invalid_client_id);
    void enqueue_message_for_clients(out_message&& message, client_id id = invalid_client_id, uint8_t flags = 0, client_id source_id = invalid_client_id);

    void handle_do_handshake_for_client(client_id id);
    void close_io();

    std::mutex m_mutex;
    state m_state;

    clock_ptr m_clock;
    std::shared_ptr<storage::storage> m_storage;
    diagnostics::event_dispatcher_ptr m_diagnostics_dispatcher;
    uint16_t m_bind_port;

    looper::loop m_loop;

    looper::tcp_server_holder m_tcp;
    message_parser m_parser;
    message_queue m_message_queue;
    uint64_t m_next_message_id;

    client_id m_next_client_id;
    storage::entry_id m_next_entry_id;
    std::map<client_id, std::unique_ptr<server_client>> m_clients;
    std::map<storage::entry_id, std::string> m_id_assignments;

    looper::timer_holder m_update_timer_handle;
    timer m_open_retry_timer;
};

}
