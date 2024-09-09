#pragma once

#include <chrono>
#include <iterator>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <deque>
#include <condition_variable>

#include "obsr_types.h"
#include "os/io.h"
#include "util/handles.h"
#include "os/signal.h"

namespace obsr::events {

using event_types = uint32_t;

enum event_type : event_types {
    event_in = (0x1 << 0),
    event_out = (0x1 << 1),
    event_error = (0x1 << 2),
    event_hung = (0x1 << 3)
};

class event_data {
public:
    virtual ~event_data() = default;

    virtual size_t count() const = 0;

    virtual os::descriptor get_descriptor(size_t index) const = 0;
    virtual event_types get_events(size_t index) const = 0;
};

class polled_events final {
public:
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::pair<os::descriptor, event_types>;

        iterator(event_data* data, size_t index)
            : m_data(data)
            , m_index(index)
        {}

        value_type operator*() const {
            auto descriptor = m_data->get_descriptor(m_index);
            auto events = m_data->get_events(m_index);
            return {descriptor, events};
        }

        iterator& operator++() {
            m_index++;
            return *this;
        }

        friend bool operator==(const iterator& a, const iterator& b) {
            return a.m_index == b.m_index;
        }
        friend bool operator!=(const iterator& a, const iterator& b) {
            return a.m_index != b.m_index;
        }

    private:
        event_data* m_data;
        size_t m_index;
    };

    explicit polled_events(event_data* data);

    iterator begin() const;
    iterator end() const;

private:
    event_data* m_data;
};

class poller {
public:
    virtual ~poller() = default;

    virtual void add(os::resource& resource, event_types events) = 0;
    virtual void set(os::resource& resource, event_types events) = 0;
    virtual void remove(os::resource& resource) = 0;

    virtual polled_events poll(size_t max_events, std::chrono::milliseconds timeout) = 0;
};

class looper final {
public:
    using generic_callback = std::function<void(looper&)>;
    using io_callback = std::function<void(looper&, obsr::handle, event_types)>;
    using timer_callback = std::function<void(looper&, obsr::handle)>;

    enum class events_update_type {
        override,
        append,
        remove
    };
    enum class execute_type {
        async,
        sync
    };

    explicit looper(std::unique_ptr<poller>&& poller);
    looper();

    void signal_run();

    obsr::handle add(std::shared_ptr<os::resource> resource, event_types events, io_callback callback);
    // note: calling this from another thread while a callback is running causes a race with a potential
    // for crashing.
    void remove(obsr::handle handle);
    void request_updates(obsr::handle handle, event_types events, events_update_type type = events_update_type::override);

    obsr::handle create_timer(std::chrono::milliseconds timeout, timer_callback callback);
    void stop_timer(obsr::handle handle);

    void request_execute(generic_callback callback, execute_type type = execute_type::async);

    void loop();

private:
    struct resource_data {
        obsr::handle handle;
        std::shared_ptr<obsr::os::resource> resource;
        event_types events;
        io_callback callback;
    };
    struct timer_data {
        std::chrono::milliseconds timeout;
        timer_callback callback;
        std::chrono::milliseconds next_timestamp;
    };
    enum class update_type {
        add,
        new_events,
        new_events_add,
        new_events_remove,
    };
    struct update {
        obsr::handle handle;
        update_type type;
        event_types events;
    };
    struct execute_request {
        generic_callback callback;
    };

    void process_updates();
    void process_update(update& update);
    void process_events(std::unique_lock<std::mutex>& lock, polled_events& events);
    void process_timers(std::unique_lock<std::mutex>& lock);
    void execute_requests(std::unique_lock<std::mutex>& lock);

    std::mutex m_mutex;
    std::condition_variable m_loop_finish;
    std::unique_ptr<poller> m_poller;
    handle_table<resource_data, 256> m_handles;
    std::unordered_map<os::descriptor, resource_data*> m_fd_map;
    std::deque<update> m_updates;
    std::deque<execute_request> m_execute_requests;
    std::shared_ptr<os::signal> m_run_signal;
    handle_table<timer_data, 16> m_timer_handles;
    std::chrono::milliseconds m_timeout;
};

class looper_thread final {
public:
    explicit looper_thread(std::shared_ptr<looper>& looper);
    ~looper_thread();

private:
    void thread_main();

    std::shared_ptr<looper> m_looper;

    std::atomic<bool> m_thread_loop_run;
    std::thread m_thread;
};

}
