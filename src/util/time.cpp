
#include "internal_except.h"

#include "time.h" // NOLINT(*-deprecated-headers)

namespace obsr {

clock::clock()
    : m_offset(std::chrono::milliseconds(0))
{}

void clock::sync(const sync_data& data) {
    const auto now = this->now();
    const auto offset = ((data.remote_start - data.us_start) + (data.remote_end - now)) / 2;

    m_offset.store(offset);
}

std::chrono::milliseconds clock::now() {
    const auto offset = m_offset.load();
    return time_now() - offset;
}

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
