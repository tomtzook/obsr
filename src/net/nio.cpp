
#include "debug.h"
#include "nio.h"


#define LOG_MODULE "niorunner"

namespace obsr::net {

nio_runner::nio_runner()
    : m_handles()
    , m_new_resources()
    , m_deleted_resources()
    , m_selector()
    , m_thread_loop_run(true)
    , m_mutex()
    , m_thread(&nio_runner::thread_main, this) {

}

nio_runner::~nio_runner() {
    m_thread_loop_run.store(false);
    m_thread.join();
}

obsr::handle nio_runner::add(std::shared_ptr<obsr::os::resource> resource, uint32_t flags, callback callback) {
    std::unique_lock guard(m_mutex);

    auto handle = m_handles.allocate_new(resource, flags, std::move(callback));
    TRACE_DEBUG(LOG_MODULE, "added new resource (handle 0x%x) for flags 0x%X", handle, flags);

    m_new_resources.push_back(handle);

    return handle;
}

void nio_runner::remove(obsr::handle handle) {
    std::unique_lock guard(m_mutex);

   auto data = m_handles.release(handle);
    TRACE_DEBUG(LOG_MODULE, "resource (handle 0x%x) starting removal", handle);

    m_deleted_resources.push_back(data->selector_handle);
}

void nio_runner::thread_main() {
    obsr::os::selector::poll_result poll_result{};

    while (m_thread_loop_run.load()) {
        std::unique_lock lock(m_mutex);

        process_removed_resources();
        process_new_resources();

        lock.unlock();
        m_selector.poll(poll_result, std::chrono::milliseconds(1000));
        lock.lock();

        handle_poll_result(lock, poll_result);
    }
}

void nio_runner::process_removed_resources() {
    auto it = m_deleted_resources.begin();
    while (it != m_deleted_resources.end()) {
        auto handle = *it;

        try {
            m_selector.remove(handle);
        } catch (...) {
            // todo: what? retry or give up?
        }

        m_deleted_resources.erase(it);
    }
}

void nio_runner::process_new_resources() {
    auto it = m_new_resources.begin();
    while (it != m_new_resources.end()) {
        auto handle = *it;

        try {
            auto data = m_handles[handle];
            data->selector_handle = m_selector.add(data->r_resource, data->r_flags);
        } catch (...) {
            // todo: what? retry or give up?
        }

        m_new_resources.erase(it);
    }
}

void nio_runner::handle_poll_result(std::unique_lock<std::mutex>& lock, obsr::os::selector::poll_result& result) {
    for (auto [handle, data] : m_handles) {
        const auto polled_flags = result.get(data.selector_handle);
        const auto adjusted_flags = (polled_flags & data.r_flags);
        if (adjusted_flags == 0) {
            continue;
        }

        TRACE_DEBUG(LOG_MODULE, "resource (handle 0x%x) received events flags=0x%x", handle, adjusted_flags);

        lock.unlock();
        try {
            data.r_callback(adjusted_flags);
        } catch (...) {}
        lock.lock();
    }
}

}
