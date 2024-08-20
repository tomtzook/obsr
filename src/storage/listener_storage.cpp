
#include <utility>

#include "util/time.h"
#include "listener_storage.h"

namespace obsr::storage {

listener_data::listener_data(listener_callback callback, const std::string_view& prefix)
    : m_callback(std::move(callback))
    , m_prefix(prefix)
    , m_creation_timestamp(time_now()) {
}

bool listener_data::in_path(const std::string_view& path) const {
    return m_prefix.find(path) >= 0;
}

void listener_data::invoke(const event& event) const {
    if (event.timestamp < m_creation_timestamp) {
        return;
    }
    if (event.path.find(m_prefix) < 0) {
        return;
    }

    m_callback(event);
}

listener_storage::listener_storage()
    : m_listeners()
    , m_thread_loop_run(true)
    , m_mutex()
    , m_has_events()
    , m_pending_events()
    , m_thread(&listener_storage::thread_main, this) {

}

listener_storage::~listener_storage() {
    m_thread_loop_run.store(false);
    m_has_events.notify_all();
    m_thread.join();
}

listener listener_storage::create_listener(const listener_callback& callback, const std::string_view& prefix) {
    std::unique_lock guard(m_mutex);

    return m_listeners.allocate_new(callback, prefix);
}

void listener_storage::destroy_listener(listener listener) {
    std::unique_lock guard(m_mutex);

    m_listeners.release(listener);
}

void listener_storage::destroy_listeners(const std::string_view& path) {
    std::unique_lock guard(m_mutex);

    std::vector<listener> handles;
    for (auto [handle, data] : m_listeners) {
        if (data.in_path(path)) {
            handles.push_back(handle);
        }
    }

    for (auto handle : handles) {
        m_listeners.release(handle);
    }
}

void listener_storage::notify(event_type type, const std::string_view& path) {
    obsr::event event;
    event.timestamp = time_now();
    event.type = type;
    event.path = path;

    notify(event);
}

void listener_storage::notify(event_type type, const std::string_view& path, const value_t& old_value, const value_t& new_value) {
    obsr::event event;
    event.timestamp = time_now();
    event.type = type;
    event.path = path;
    event.old_value = old_value;
    event.value = new_value;

    notify(event);
}

void listener_storage::notify(const event& event) {
    std::unique_lock guard(m_mutex);

    m_pending_events.push_back(event);
    m_has_events.notify_all();
}

void listener_storage::thread_main() {
    while (m_thread_loop_run.load()) {
        std::unique_lock lock(m_mutex);

        m_has_events.wait(lock, [&]()->bool {
            return !m_pending_events.empty() || !m_thread_loop_run.load();
        });

        if (!m_thread_loop_run.load()) {
            break;
        }

        for (auto& event : m_pending_events) {
            for (auto [handle, listener] : m_listeners) {
                lock.unlock();
                try {
                    listener.invoke(event);
                } catch (...) {}
                lock.lock();
            }
        }
        m_pending_events.clear();

    }
}

}
