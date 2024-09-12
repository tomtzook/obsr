
#include <utility>

#include "debug.h"
#include "internal_except.h"
#include "os/signal.h"
#include "events.h"
#include "util/time.h"
#include "os/poller.h"

namespace obsr::events {

#define LOG_MODULE "looper"

static constexpr auto initial_poll_timeout = std::chrono::milliseconds(1000);
static constexpr auto min_poll_timeout = std::chrono::milliseconds(100);
static constexpr size_t max_events_for_process = 20;

polled_events::polled_events(event_data* data)
    : m_data(data)
{}

polled_events::iterator polled_events::begin() const {
    return {m_data, 0};
}

polled_events::iterator polled_events::end() const {
    return {m_data, m_data->count()};
}

looper::looper(std::unique_ptr<poller>&& poller)
    : m_mutex()
    , m_loop_finish()
    , m_poller(std::move(poller))
    , m_handles()
    , m_fd_map()
    , m_updates()
    , m_execute_requests()
    , m_run_signal(std::make_shared<os::signal>())
    , m_timer_handles()
    , m_timeout(initial_poll_timeout) {
    add(m_run_signal, event_in, [this](looper& looper, obsr::handle handle, event_types events)->void {
        m_run_signal->clear();
    });
}

looper::looper()
    : looper(std::make_unique<os::resource_poller>())
{}

void looper::signal_run() {
    m_run_signal->set();
}

obsr::handle looper::add(std::shared_ptr<os::resource> resource, event_types events, io_callback callback) {
    std::unique_lock lock(m_mutex);

    const auto descriptor = resource->get_descriptor();

    auto it = m_fd_map.find(descriptor);
    if (it != m_fd_map.end()) {
        throw illegal_argument_exception("resource already added");
    }

    auto handle = m_handles.allocate_new();
    auto data = m_handles[handle];
    data->handle = handle;
    data->resource = std::move(resource);
    data->events = 0;
    data->callback = std::move(callback);

    m_fd_map.emplace(descriptor, data);
    m_updates.push_back({handle, update_type::add, events});

    signal_run();

    return handle;
}

void looper::remove(obsr::handle handle) {
    std::unique_lock lock(m_mutex);

    if (!m_handles.has(handle)) {
        throw no_such_handle_exception(handle);
    }

    auto data = m_handles.release(handle);
    m_fd_map.erase(data->resource->get_descriptor());
    m_poller->remove(*data->resource);

    signal_run();
}

void looper::request_updates(obsr::handle handle, event_types events, events_update_type type) {
    std::unique_lock lock(m_mutex);

    if (!m_handles.has(handle)) {
        throw no_such_handle_exception(handle);
    }

    update_type update_type;
    switch (type) {
        case events_update_type::override:
            update_type = update_type::new_events;
            break;
        case events_update_type::append:
            update_type = update_type::new_events_add;
            break;
        case events_update_type::remove:
            update_type = update_type::new_events_remove;
            break;
        default:
            throw std::runtime_error("unsupported event type");
    }

    m_updates.push_back({handle, update_type, events});
    signal_run();
}

obsr::handle looper::create_timer(std::chrono::milliseconds timeout, timer_callback callback) {
    std::unique_lock lock(m_mutex);

    if (timeout < min_poll_timeout) {
        throw illegal_argument_exception("timeout too small");
    }

    auto handle = m_timer_handles.allocate_new();
    auto data = m_timer_handles[handle];
    data->timeout = timeout;
    data->callback = std::move(callback);
    data->next_timestamp = time_now() + timeout;

    if (m_timeout > timeout) {
        m_timeout = timeout;
    }

    signal_run();

    return handle;
}

void looper::stop_timer(obsr::handle handle) {
    std::unique_lock lock(m_mutex);

    if (!m_handles.has(handle)) {
        throw no_such_handle_exception(handle);
    }

    auto data = m_timer_handles[handle];
    data->timeout = std::chrono::milliseconds(0);
}

void looper::request_execute(generic_callback callback, execute_type type) {
    std::unique_lock lock(m_mutex);

    execute_request request{};
    request.callback = std::move(callback);
    m_execute_requests.push_back(request);

    signal_run();

    if (type == execute_type::sync) {
        // this is kind of a cheat since we wait for everything in the loop
        // to run
        m_loop_finish.wait(lock, [&]()->bool {
            return m_execute_requests.empty();
        });
    }
}

void looper::loop() {
    std::unique_lock lock(m_mutex);

    process_updates();

    lock.unlock();
    auto result = m_poller->poll(max_events_for_process, m_timeout);
    lock.lock();

    process_events(lock, result);
    process_timers(lock);
    execute_requests(lock);

    m_loop_finish.notify_all();
}

void looper::process_updates() {
    while (!m_updates.empty()) {
        auto& update = m_updates.front();
        process_update(update);

        m_updates.pop_front();
    }
}

void looper::process_update(update& update) {
    if (!m_handles.has(update.handle)) {
        return;
    }

    auto data = m_handles[update.handle];

    switch (update.type) {
        case update_type::add:
            data->events = update.events;
            m_poller->add(*data->resource, data->events);
            break;
        case update_type::new_events:
            data->events = update.events;
            m_poller->set(*data->resource, data->events);
            break;
        case update_type::new_events_add:
            data->events |= update.events;
            m_poller->set(*data->resource, data->events);
            break;
        case update_type::new_events_remove:
            data->events &= ~update.events;
            m_poller->set(*data->resource, data->events);
            break;
    }
}

void looper::process_events(std::unique_lock<std::mutex>& lock, polled_events& events) {
    for (auto [fd, revents] : events) {
        auto it = m_fd_map.find(fd);
        if (it == m_fd_map.end()) {
            continue;
        }

        auto data = it->second;

        auto adjusted_flags = (data->events & revents);
        if (adjusted_flags == 0) {
            continue;
        }

        lock.unlock();
        try {
            data->callback(*this, data->handle, adjusted_flags);
        } catch (const std::exception& e) {
            TRACE_ERROR(LOG_MODULE, "Error in io callback: what=%s", e.what());
        } catch (...) {
            TRACE_ERROR(LOG_MODULE, "Error in io callback: unknown");
        }
        lock.lock();
    }
}

void looper::process_timers(std::unique_lock<std::mutex>& lock) {
    std::vector<obsr::handle> to_remove;
    const auto now = time_now();

    for (auto [handle, data] : m_timer_handles) {
        if (data.timeout.count() == 0) {
            to_remove.push_back(handle);
            continue;
        }

        if (data.next_timestamp > now) {
            continue;
        }

        lock.unlock();
        try {
            data.callback(*this, handle);
        } catch (const std::exception& e) {
            TRACE_ERROR(LOG_MODULE, "Error in timer callback: what=%s", e.what());
        } catch (...) {
            TRACE_ERROR(LOG_MODULE, "Error in timer callback: unknown");
        }
        lock.lock();

        data.next_timestamp = now + data.timeout;
    }

    for (auto handle : to_remove) {
        m_timer_handles.release(handle);
    }
}

void looper::execute_requests(std::unique_lock<std::mutex>& lock) {
    while (!m_execute_requests.empty()) {
        auto& request = m_execute_requests.front();

        lock.unlock();
        try {
            request.callback(*this);
        } catch (const std::exception& e) {
            TRACE_ERROR(LOG_MODULE, "Error in request callback: what=%s", e.what());
        } catch (...) {
            TRACE_ERROR(LOG_MODULE, "Error in request callback: unknown");
        }
        lock.lock();

        m_execute_requests.pop_front();
    }
}

looper_thread::looper_thread(std::shared_ptr<looper>& looper)
    : m_looper(looper)
    , m_thread_loop_run(true)
    , m_thread(&looper_thread::thread_main, this)
{}

looper_thread::~looper_thread() {
    m_thread_loop_run.store(false);
    // force looper to run
    m_looper->signal_run();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void looper_thread::thread_main() {
    while (m_thread_loop_run.load()) {
        m_looper->loop();
    }
}

}
