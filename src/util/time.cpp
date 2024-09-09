
#include "time.h" // NOLINT(*-deprecated-headers)

#include "internal_except.h"
#include "debug.h"

namespace obsr {

#define LOG_MODULE "send_time"

clock::clock()
    : m_offset(std::chrono::milliseconds(0))
    , m_rtt2(std::chrono::milliseconds(INT64_MAX))
{}

bool clock::sync(std::chrono::milliseconds local_time, std::chrono::milliseconds remote_time) {
    const auto now = this->now();
    const auto rtt2 = (now - local_time) / 2;
    if (rtt2 >= m_rtt2.load()) {
        return false;
    }

    const auto offset = remote_time + rtt2 - now;

    m_rtt2.store(rtt2);
    m_offset.store(offset);

    TRACE_DEBUG(LOG_MODULE, "new clock offset: offset=%lu, old send_time=%lu, new send_time=%lu",
               offset, now, this->now());

    return true;
}

std::chrono::milliseconds clock::adjust_time(std::chrono::milliseconds time) {
    return time - m_offset.load();
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
        throw illegal_state_exception("expected time bigger than 0");
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
