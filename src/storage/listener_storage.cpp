
#include <utility>

#include "listener_storage.h"

namespace obsr::storage {

static inline std::chrono::milliseconds now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());
}

listener_data::listener_data(listener_callback callback, const std::string_view& prefix)
    : m_callback(std::move(callback))
    , m_prefix(prefix)
    , m_creation_timestamp(now()) {
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
    , m_mutex()
    , m_has_events()
    , m_pending_events()
    , m_thread(&listener_storage::thread_main, this) {

}

listener listener_storage::create_listener(const listener_callback& callback, const std::string_view& prefix) {
    std::unique_lock guard(m_mutex);

    return m_listeners.allocate_new(callback, prefix);
}

void listener_storage::destroy_listener(listener listener) {
    std::unique_lock guard(m_mutex);

    m_listeners.release(listener);
}

void listener_storage::destroy_listeners_in_path(const std::string_view& path) {
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

void listener_storage::notify(const event& event) {
    // todo: construct event object here
    std::unique_lock guard(m_mutex);

    obsr::event our_event(event);
    our_event.timestamp = now();

    m_pending_events.push_back(our_event);
    m_has_events.notify_all();
}

void listener_storage::thread_main() {
    while (true) {
        std::unique_lock lock(m_mutex);

        m_has_events.wait(lock, [&]()->bool {
            return !m_pending_events.empty();
        });

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
