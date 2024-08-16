#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <atomic>

#include "os/io.h"
#include "util/handles.h"

namespace obsr::io {

// todo: simplify selected without its complext state
//      pass to it structs with params from here
// todo: integrate poll into resources where they can tell us which poll type to use and meaning

class nio_runner {
public:
    using callback = std::function<void(obsr::os::resource& resource, uint32_t flags)>;

    nio_runner();
    ~nio_runner();

    obsr::handle add(std::shared_ptr<obsr::os::resource> resource, uint32_t flags, callback callback);
    void remove(obsr::handle handle);

    void add_flags(obsr::handle handle, uint32_t flags);
    void remove_flags(obsr::handle handle, uint32_t flags);

private:
    void adjust_flags(obsr::handle handle, uint32_t flags);

    void thread_main();

    void handle_updates();
    void handle_poll_results(std::unique_lock<std::mutex>& lock);

    struct resource_data {
    public:
        resource_data(std::shared_ptr<obsr::os::resource>& resource, uint32_t flags, callback& callback);
        ~resource_data();

        uint32_t flags() const;
        void flags(uint32_t flags);

        void synchronize_resource();
        void register_with_selector(obsr::os::selector& selector);
        void unregister_with_selector(obsr::os::selector& selector);

        void invoke_callback(std::unique_lock<std::mutex>& lock);

    private:
        std::shared_ptr<obsr::os::resource> m_resource;
        uint32_t m_flags;
        callback m_callback;
        obsr::os::selector::poll_resource* m_selector_resource;
    };

    enum class update_type {
        added,
        removed,
        flags_changed
    };

    handle_table<resource_data, 16> m_handles;
    std::vector<std::tuple<obsr::handle, update_type>> m_updated;

    obsr::os::selector m_selector;

    std::atomic<bool> m_thread_loop_run;
    std::mutex m_mutex;
    std::thread m_thread;
};

}
