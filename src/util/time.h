#pragma once

#include <chrono>
#include <atomic>

namespace obsr {

class clock {
public:
    clock();

    void sync(std::chrono::milliseconds local_time, std::chrono::milliseconds remote_time);

    std::chrono::milliseconds now();

private:
    std::atomic<std::chrono::milliseconds> m_offset;
};

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
