
#include "network.h"

namespace obsr::diagnostics {

network_monitor::network_monitor(event_dispatcher_ptr dispatcher)
    : m_mutex()
    , m_dispatcher(std::move(dispatcher))
    , m_data()
{}

network_monitor::~network_monitor() {

}

std::vector<network_monitor::message> network_monitor::get_data_snapshot(const size_t count, const direction direction) {
    std::array<message, history_size> copy;
    uint64_t write_index;
    size_t available_count;
    {
        std::unique_lock lock(m_mutex);
        m_data.copy_into(copy, write_index, available_count);
    }

    const auto count_get = std::min(count, available_count);

    std::vector<message> return_data;
    return_data.reserve(count_get);

    for (int i = 0; i < count_get; i++) {
        const auto index = (write_index - i) % history_size;
        auto& data = copy[index];

        if (direction != direction::any && data.direction != direction) {
            continue;
        }

        return_data.push_back(std::move(data));
    }

    return std::move(return_data);
}

void network_monitor::start() {
    std::unique_lock lock(m_mutex);

    // todo: need to detach listener!
    m_dispatcher->listen([this](const auto& event)->void {
        on_event(event);
    });
}

void network_monitor::on_event(const diagnostic_event& event) {
    if (event.has<network_received_message>()) {
        const auto& data = event.get<network_received_message>();
        message message{direction::in, data.type, data.client_id, data.message_id};

        std::unique_lock lock(m_mutex);
        m_data.add(std::move(message));
    } else if (event.has<network_sending_message>()) {
        const auto& data = event.get<network_sending_message>();
        message message{direction::out, data.type, data.client_id, data.message_id};

        std::unique_lock lock(m_mutex);
        m_data.add(std::move(message));
    }
}

}
