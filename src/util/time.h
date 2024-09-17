#pragma once

#include <chrono>
#include <atomic>
#include <memory>

namespace obsr {

class clock {
public:
    clock();

    bool sync(std::chrono::milliseconds local_time, std::chrono::milliseconds remote_time);
    std::chrono::milliseconds adjust_time(std::chrono::milliseconds time);

    std::chrono::milliseconds now();

private:
    std::atomic<std::chrono::milliseconds> m_offset;
    std::atomic<std::chrono::milliseconds> m_rtt2;
};

using clock_ref = std::shared_ptr<clock>;

class timer {
public:
    timer();

    bool is_running() const;
    bool has_elapsed(std::chrono::milliseconds time) const;

    void start();
    void reset();
    void stop();

private:
    std::chrono::milliseconds m_start;
};

std::chrono::milliseconds time_now();

}
