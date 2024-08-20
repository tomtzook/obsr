
#include "util/time.h"

#include "updater.h"

namespace obsr {

updater::updater()
    : m_handles()
    , m_min_period(-1)
    , m_mutex()
    , m_thread_loop_run(true)
    , m_thread(&updater::thread_main, this)
{}

updater::~updater() {
    m_thread_loop_run.store(false);
    m_can_run.notify_all();
    m_thread.join();
}

obsr::handle updater::attach(std::shared_ptr<updatable> updatable, std::chrono::milliseconds period) {
    std::unique_lock lock(m_mutex);

    auto handle=  m_handles.allocate_new(std::move(updatable), period);
    if (period < m_min_period) {
        m_min_period = period;
    }

    m_can_run.notify_all();

    return handle;
}

void updater::remove(obsr::handle handle) {
    m_handles.release(handle);
}

void updater::thread_main() {
    while (m_thread_loop_run.load()) {
        std::unique_lock lock(m_mutex);

        if (m_handles.empty()) {
            m_can_run.wait(lock, [&]()->bool {
                return !m_handles.empty() || !m_thread_loop_run.load();
            });
        } else {
            m_can_run.wait_for(lock, m_min_period);
        }

        if (!m_thread_loop_run.load()) {
            break;
        }

        const auto now = time_now();
        for (auto [handle, updatable] : m_handles) {
            if (updatable.should_be_called(now)) {
                lock.unlock();
                updatable();
                lock.lock();

                updatable.update_called(now);
            }
        }
    }
}

updater::updatable_data::updatable_data(std::shared_ptr<updatable> updatable, std::chrono::milliseconds period)
    : m_updatable(std::move(updatable))
    , m_period(period)
    , m_last_called(0)
{}

bool updater::updatable_data::should_be_called(std::chrono::milliseconds now) const {
    return now - m_last_called >= m_period;
}

void updater::updatable_data::update_called(std::chrono::milliseconds now) {
    m_last_called = now;
}

void updater::updatable_data::operator()() {
    m_updatable->update();
}

}
