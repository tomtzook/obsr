
#include "internal_except.h"

#include "time.h" // NOLINT(*-deprecated-headers)

namespace obsr {

timer::timer()
    : m_start(0)
{}

bool timer::is_running() const {
    return m_start.count() > 0;
}

bool timer::has_elapsed(std::chrono::milliseconds time) const {
    if (m_start.count() < 1) {
        throw illegal_state_exception();
    }

    const auto now = time_now();
    return now - m_start >= time;
}

void timer::start() {
    m_start = time_now();
}

void timer::reset() {
    m_start = time_now();
}

void timer::stop() {
    m_start = std::chrono::milliseconds(0);
}

std::chrono::milliseconds time_now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());
}

}
