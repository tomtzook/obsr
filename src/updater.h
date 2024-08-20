#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <memory>
#include <condition_variable>

#include "util/handles.h"

namespace obsr {

class updatable {
public:
    virtual void update() = 0;
};

class updater {
public:
    updater();
    ~updater();

    obsr::handle attach(std::shared_ptr<updatable> updatable, std::chrono::milliseconds period);
    void remove(obsr::handle handle);

private:
    void thread_main();

    struct updatable_data {
    public:
        updatable_data(std::shared_ptr<updatable> updatable, std::chrono::milliseconds period);

        bool should_be_called(std::chrono::milliseconds now) const;
        void update_called(std::chrono::milliseconds now);

        void operator()();

    private:
        std::shared_ptr<updatable> m_updatable;
        std::chrono::milliseconds m_period;
        std::chrono::milliseconds m_last_called;
    };

    handle_table<updatable_data, 8> m_handles;
    std::chrono::milliseconds m_min_period;

    std::condition_variable m_can_run;
    std::atomic<bool> m_thread_loop_run;
    std::mutex m_mutex;

    std::thread m_thread;
};

}
