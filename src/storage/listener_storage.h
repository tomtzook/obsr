#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>

#include "obsr_types.h"
#include "obsr_internal.h"
#include "util/handles.h"


namespace obsr::storage {

struct listener_data {
    listener_data(listener_callback callback, const std::string_view& prefix);

    bool in_path(const std::string_view& path) const;
    void invoke(const event& event) const;

private:
    listener_callback m_callback;
    std::string m_prefix;
    std::chrono::milliseconds m_creation_timestamp;
};

class listener_storage {
public:
    listener_storage();

    listener create_listener(const listener_callback& callback, const std::string_view& prefix);
    void destroy_listener(listener listener);
    void destroy_listeners_in_path(const std::string_view& path);

    void notify(const event& event);

private:
    void thread_main();

    handle_table<listener_data, 16> m_listeners;

    std::mutex m_mutex;
    std::condition_variable m_has_events;
    std::deque<event> m_pending_events;

    std::thread m_thread;
};

using listener_storage_ref = std::shared_ptr<listener_storage>;

}
