#pragma once

#include "diagnostics/dispatcher.h"
#include "diagnostics/data.h"

namespace obsr::diagnostics {

class network_monitor {
public:
    static constexpr size_t history_size = 1024;

    struct message {
        network_message::data_direction direction;
        net::message_type type;
        uint16_t client_id;
        uint64_t message_id;
        std::chrono::milliseconds timestamp;
        network_message::message_data data;
    };
    using filter_func = std::function<bool(const message&)>;

    explicit network_monitor(event_dispatcher_ptr dispatcher);
    ~network_monitor();

    std::vector<message> get_data_snapshot(size_t count = history_size, filter_func&& filter = nullptr);
    std::map<uint16_t, net::connection_info> get_connections();

    void start();

private:
    void on_event(const diagnostic_event& event);

    std::mutex m_mutex;
    event_dispatcher_ptr m_dispatcher;
    non_blocking_circular_buffer<message, history_size> m_data;
    std::map<uint16_t, net::connection_info> m_connections;
};

}
