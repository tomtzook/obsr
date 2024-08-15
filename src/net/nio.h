#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <atomic>

#include "os/io.h"
#include "util/handles.h"

namespace obsr::net {

// todo: simplify selected without its complext state
//      pass to it structs with params from here
// todo: integrate poll into resources where they can tell us which poll type to use and meaning

class nio_runner {
public:
    using callback = std::function<void(uint32_t flags)>;

    nio_runner();
    ~nio_runner();

    obsr::handle add(std::shared_ptr<obsr::os::resource> resource, uint32_t flags, callback callback);
    void remove(obsr::handle handle);

private:
    void thread_main();
    void process_removed_resources();
    void process_new_resources();
    void handle_poll_result(std::unique_lock<std::mutex>& lock, obsr::os::selector::poll_result& result);

    struct resource_data {
        resource_data(std::shared_ptr<obsr::os::resource>& resource, uint32_t flags, callback& callback)
            : r_resource(resource)
            , r_flags(flags)
            , r_callback(std::move(callback))
            , selector_handle(empty_handle)
        {}

        std::shared_ptr<obsr::os::resource> r_resource;
        uint32_t r_flags;
        callback r_callback;
        obsr::handle selector_handle;
    };

    handle_table<resource_data, 16> m_handles;
    std::vector<obsr::handle> m_new_resources;
    std::vector<obsr::handle> m_deleted_resources;

    obsr::os::selector m_selector;

    std::atomic<bool> m_thread_loop_run;
    std::mutex m_mutex;
    std::thread m_thread;
};

}
