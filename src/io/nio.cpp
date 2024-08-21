
#include "debug.h"
#include "nio.h"


#define LOG_MODULE "niorunner"

namespace obsr::io {

nio_runner::resource_data::resource_data(std::shared_ptr<obsr::os::resource>& resource, uint32_t flags, callback& callback)
    : m_resource(resource)
    , m_flags(flags)
    , m_callback(std::move(callback))
    , m_selector_resource(nullptr)
{}

nio_runner::resource_data::~resource_data() {

}

uint32_t nio_runner::resource_data::flags() const {
    return m_flags;
}

void nio_runner::resource_data::flags(uint32_t flags) {
    m_flags = flags;
}

void nio_runner::resource_data::synchronize_resource() {
    if (m_selector_resource != nullptr) {
        m_selector_resource->flags(m_flags);
    }
}

void nio_runner::resource_data::register_with_selector(obsr::os::selector& selector) {
    m_selector_resource = selector.add(m_resource, m_flags);
}

void nio_runner::resource_data::unregister_with_selector(obsr::os::selector& selector) {
    if (m_selector_resource != nullptr) {
        selector.remove(m_selector_resource);
        m_selector_resource = nullptr;
    }
}

void nio_runner::resource_data::invoke_callback(std::unique_lock<std::mutex>& lock) {
    if (m_selector_resource == nullptr) {
        return;
    }

    auto matched_flags = m_selector_resource->result_flags();
    auto adjusted_flags = (matched_flags & m_flags);
    if (adjusted_flags == 0) {
        return;
    }

    TRACE_DEBUG(LOG_MODULE, "invoking resource callback with flags 0x%x", adjusted_flags);
    lock.unlock();
    try {
        m_callback(*m_resource, adjusted_flags);
    } catch (...) {}
    lock.lock();
}

nio_runner::nio_runner()
    : m_handles()
    , m_updated()
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

    m_updated.emplace_back(handle, update_type::added);

    return handle;
}

void nio_runner::remove(obsr::handle handle) {
    std::unique_lock guard(m_mutex);

    TRACE_DEBUG(LOG_MODULE, "resource (handle 0x%x) starting removal", handle);
    m_updated.emplace_back(handle, update_type::removed);
}

void nio_runner::add_flags(obsr::handle handle, uint32_t flags) {
    std::unique_lock guard(m_mutex);

    auto data = m_handles[handle];
    adjust_flags(handle, data->flags() | flags);
}

void nio_runner::remove_flags(obsr::handle handle, uint32_t flags) {
    std::unique_lock guard(m_mutex);

    auto data = m_handles[handle];
    adjust_flags(handle, data->flags() & ~flags);
}

void nio_runner::adjust_flags(obsr::handle handle, uint32_t flags) {
    auto data = m_handles[handle];
    if (data->flags() == flags) {
        return;
    }

    data->flags(flags);

    TRACE_DEBUG(LOG_MODULE, "resource (handle 0x%x) flags updated to 0x%x", handle, flags);
    m_updated.emplace_back(handle, update_type::flags_changed);
}

void nio_runner::thread_main() {
    while (m_thread_loop_run.load()) {
        std::unique_lock lock(m_mutex);

        handle_updates();

        lock.unlock();
        m_selector.poll(std::chrono::milliseconds(1000));
        lock.lock();

        handle_poll_results(lock);
    }
}

void nio_runner::handle_updates() {
    while (!m_updated.empty()) {
        auto [handle, type] = m_updated.front();

        switch (type) {
            case update_type::added: {
                auto data = m_handles[handle];
                data->register_with_selector(m_selector);
                TRACE_DEBUG(LOG_MODULE, "finished configuring resource (handle 0x%x)", handle);
                break;
            }
            case update_type::removed: {
                auto data = m_handles.release(handle);
                data->unregister_with_selector(m_selector);
                TRACE_DEBUG(LOG_MODULE, "finished removing resource (handle 0x%x)", handle);
                break;
            }
            case update_type::flags_changed: {
                TRACE_DEBUG(LOG_MODULE, "synchronizing (handle 0x%x)", handle);
                auto data = m_handles[handle];
                data->synchronize_resource();
                break;
            }
        }

        m_updated.pop_front();
    }
}

void nio_runner::handle_poll_results(std::unique_lock<std::mutex>& lock) {
    for (auto [handle, data] : m_handles) {
        data.invoke_callback(lock);
    }
}

}
