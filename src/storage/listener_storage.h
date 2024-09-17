#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>

#include "obsr_types.h"
#include "obsr_internal.h"
#include "util/handles.h"


namespace obsr::storage {

struct listener_data {
    listener_data(listener_callback callback, const std::string_view& prefix,
                  std::chrono::milliseconds creation_timestamp);

    bool in_path(const std::string_view& path) const;

    std::chrono::milliseconds get_creation_timestamp() const;
    void set_creation_timestamp(std::chrono::milliseconds creation_timestamp);

    void invoke(const event& event) const;

private:
    listener_callback m_callback;
    std::string m_prefix;
    std::chrono::milliseconds m_creation_timestamp;
};

class listener_storage {
public:
    listener_storage(clock_ref  clock);
    ~listener_storage();

    void on_clock_resync();

    listener create_listener(const listener_callback& callback, const std::string_view& prefix);
    void destroy_listener(listener listener);
    void destroy_listeners(const std::string_view& path);

    void notify(event_type type, const std::string_view& path, obsr::entry entry);
    void notify(event_type type, const std::string_view& path, obsr::entry entry,
                const value& old_value, const value& new_value);

private:
    void notify(const event& event);
    void thread_main();

    clock_ref m_clock;
    handle_table<listener_data, 16> m_listeners;

    std::atomic<bool> m_thread_loop_run;
    std::mutex m_mutex;
    std::condition_variable m_has_events;
    std::deque<event> m_pending_events;

    std::thread m_thread;
};

using listener_storage_ref = std::shared_ptr<listener_storage>;

}
