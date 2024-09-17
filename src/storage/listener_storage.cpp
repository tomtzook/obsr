
#include <utility>

#include "debug.h"
#include "util/time.h"
#include "listener_storage.h"

namespace obsr::storage {

#define LOG_MODULE "listener_storage"

listener_data::listener_data(listener_callback callback, const std::string_view& prefix,
                             std::chrono::milliseconds creation_timestamp)
    : m_callback(std::move(callback))
    , m_prefix(prefix)
    , m_creation_timestamp(creation_timestamp) {
}

bool listener_data::in_path(const std::string_view& path) const {
    return m_prefix.find(path) != std::string::npos;
}

std::chrono::milliseconds listener_data::get_creation_timestamp() const {
    return m_creation_timestamp;
}

void listener_data::set_creation_timestamp(std::chrono::milliseconds creation_timestamp) {
    m_creation_timestamp = creation_timestamp;
}

void listener_data::invoke(const event& event) const {
    if (event.get_timestamp() < m_creation_timestamp) {
        return;
    }
    if (event.get_path().find(m_prefix) == std::string::npos) {
        return;
    }

    m_callback(event);
}

listener_storage::listener_storage(clock_ref  clock)
    : m_clock(std::move(clock))
    , m_listeners()
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

void listener_storage::on_clock_resync() {
    std::unique_lock guard(m_mutex);

    for (auto& event : m_pending_events) {
        auto timestamp = event.get_timestamp();
        timestamp = m_clock->adjust_time(timestamp);
        event.set_timestamp(timestamp);
    }

    for (auto [handle, listener] : m_listeners) {
        auto timestamp = listener.get_creation_timestamp();
        timestamp = m_clock->adjust_time(timestamp);
        listener.set_creation_timestamp(timestamp);
    }
}

listener listener_storage::create_listener(const listener_callback& callback, const std::string_view& prefix) {
    std::unique_lock guard(m_mutex);

    return m_listeners.allocate_new(callback, prefix, m_clock->now());
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

void listener_storage::notify(event_type type, const std::string_view& path, obsr::entry entry) {
    obsr::event event(m_clock->now(), type, path, entry);
    notify(event);
}

void listener_storage::notify(event_type type, const std::string_view& path, obsr::entry entry,
                              const value& old_value, const value& new_value) {
    obsr::event event(m_clock->now(), type, path, entry, old_value, new_value);
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

        while (!m_pending_events.empty()) {
            auto& event = m_pending_events.front();

            for (auto [handle, listener] : m_listeners) {
                lock.unlock();
                try {
                    listener.invoke(event);
                } catch (const std::exception& e) {
                    TRACE_ERROR(LOG_MODULE, "Error in listener callback: what=%s", e.what());
                } catch (...) {
                    TRACE_ERROR(LOG_MODULE, "Error in listener callback: unknown");
                }
                lock.lock();
            }

            m_pending_events.pop_front();
        }
    }
}

}
