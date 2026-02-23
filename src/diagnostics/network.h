#pragma once

#include <fmt/core.h>

#include "diagnostics/dispatcher.h"
#include "diagnostics/data.h"

namespace obsr::diagnostics {

class network_monitor {
public:
    static constexpr size_t history_size = 1024;

    enum class direction {
        any = 0,
        in = 1,
        out = 2,
    };
    struct message {
        direction direction;
        net::message_type type;
        uint16_t client_id;
        uint64_t message_id;
    };

    explicit network_monitor(event_dispatcher_ptr dispatcher);
    ~network_monitor();

    std::vector<message> get_data_snapshot(size_t count = history_size, direction direction = direction::any);

    void start();

private:
    void on_event(const diagnostic_event& event);

    std::mutex m_mutex;
    event_dispatcher_ptr m_dispatcher;
    non_blocking_circular_buffer<message, history_size> m_data;
};

}
