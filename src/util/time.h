#pragma once

#include <chrono>

namespace obsr {

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
