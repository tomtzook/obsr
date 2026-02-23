#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <functional>
#include <variant>

#include "obsr_types.h"
#include "net/serialize.h"
#include "diagnostics/data.h"

namespace obsr::diagnostics {

struct storage_entry_created {
    obsr::entry handle;
    char path[256];
};

struct storage_entry_deleted {
    obsr::entry handle;
};

struct storage_entry_value_changed {
    obsr::entry handle;
    obsr::value value;
    std::chrono::milliseconds update_timestamp;
};

struct storage_entry_flags_changed {
    obsr::entry handle;
    uint16_t flags;
};

struct storage_entry_net_id_changed {
    obsr::entry handle;
    uint16_t net_id;
};

struct network_sending_message {
    net::message_type type;
    uint16_t client_id;
    uint64_t message_id;
};

struct network_received_message {
    net::message_type type;
    uint16_t client_id;
    uint64_t message_id;
};

struct diagnostic_event {
    diagnostic_event() = default;

    template<typename t_>
    bool has() const;
    template<typename t_>
    const t_& get() const;

    template<typename t_>
    void set(t_&& t);

private:
    using data_type = std::variant<
        std::monostate,
        storage_entry_created, storage_entry_deleted, storage_entry_value_changed, storage_entry_flags_changed, storage_entry_net_id_changed,
        network_sending_message, network_received_message>;
    data_type m_data;
};

class event_dispatcher {
public:
    using listener_callback = std::function<void(const diagnostic_event& event)>;

    explicit event_dispatcher();
    ~event_dispatcher();

    void listen(listener_callback&& callback);
    void notify(diagnostic_event&& event);

private:
    void thread_main();

    std::atomic<bool> m_thread_loop_run;
    std::mutex m_mutex;
    std::condition_variable m_has_events;
    std::vector<listener_callback> m_listeners;

    static constexpr size_t event_queue_size = 256;
    non_blocking_queue<diagnostic_event, 256> m_pending_events;

    std::thread m_thread;
};

using event_dispatcher_ptr = std::shared_ptr<event_dispatcher>;


void notify_entry_created(const event_dispatcher_ptr& dispatcher, obsr::handle handle, std::string_view path);
void notify_entry_deleted(const event_dispatcher_ptr& dispatcher, obsr::handle handle);
void notify_entry_value_set(const event_dispatcher_ptr& dispatcher, obsr::handle handle, const obsr::value& value);
void notify_entry_value_clear(const event_dispatcher_ptr& dispatcher, obsr::handle handle);
void notify_entry_flags_changed(const event_dispatcher_ptr& dispatcher, obsr::handle handle, uint16_t flags);
void notify_entry_net_id_set(const event_dispatcher_ptr& dispatcher, obsr::handle handle, uint16_t id);
void notify_sending_message(const event_dispatcher_ptr& dispatcher, net::message_type type, uint16_t client_id, uint64_t message_id);
void notify_received_message(const event_dispatcher_ptr& dispatcher, net::message_type type, uint16_t client_id, uint64_t message_id);

template<typename t_>
bool diagnostic_event::has() const {
    return std::holds_alternative<t_>(m_data);
}

template<typename t_>
const t_& diagnostic_event::get() const {
    return std::get<t_>(m_data);
}

template<typename t_>
void diagnostic_event::set(t_&& t) {
    m_data = std::move(t);
}

}
