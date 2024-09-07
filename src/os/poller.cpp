#include <sys/epoll.h>

#include "internal_except.h"
#include "poller.h"

namespace obsr::os {

static constexpr size_t events_buffer_size = 20;

static descriptor create() {
    const auto fd = ::epoll_create1(0);
    if (fd < 0) {
        throw io_exception(errno);
    }

    return fd;
}

static uint32_t events_to_native(events::event_types events) {
    uint32_t r_events = 0;
    if ((events & events::event_type::event_in) != 0) {
        r_events |= EPOLLIN;
    }
    if ((events & events::event_type::event_out) != 0) {
        r_events |= EPOLLOUT;
    }
    if ((events & events::event_type::event_error) != 0) {
        r_events |= EPOLLERR;
    }
    if ((events & events::event_type::event_hung) != 0) {
        r_events |= EPOLLHUP;
    }

    return r_events;
}

static events::event_types native_to_events(uint32_t events) {
    events::event_types r_events = 0;
    if ((events & EPOLLIN) != 0) {
        r_events |= events::event_type::event_in;
    }
    if ((events & EPOLLOUT) != 0) {
        r_events |= events::event_type::event_out;
    }
    if ((events & EPOLLERR) != 0) {
        r_events |= events::event_type::event_error;
    }
    if ((events & EPOLLHUP) != 0) {
        r_events |= events::event_type::event_hung;
    }

    return r_events;
}

resource_poller::resource_poller()
    : resource(create())
    , m_events(new epoll_event[events_buffer_size])
    , m_data(m_events)
{}

resource_poller::~resource_poller() {
    delete[] reinterpret_cast<epoll_event*>(m_events);
}

void resource_poller::add(resource& resource, events::event_types events) {
    const auto descriptor = resource.get_descriptor();

    epoll_event event{};
    event.events = events_to_native(events);
    event.data.fd = descriptor;

    if (::epoll_ctl(get_descriptor(), EPOLL_CTL_ADD, descriptor, &event)) {
        handle_error();
    }
}

void resource_poller::set(resource& resource, events::event_types events) {
    const auto descriptor = resource.get_descriptor();

    epoll_event event{};
    event.events = events_to_native(events);
    event.data.fd = descriptor;

    if (::epoll_ctl(get_descriptor(), EPOLL_CTL_MOD, descriptor, &event)) {
        handle_error();
    }
}

void resource_poller::remove(resource& resource) {
    const auto descriptor = resource.get_descriptor();

    epoll_event event{};
    event.events = 0;
    event.data.fd = descriptor;

    if (::epoll_ctl(get_descriptor(), EPOLL_CTL_DEL, descriptor, &event)) {
        handle_error();
    }
}

events::polled_events resource_poller::poll(size_t max_events, std::chrono::milliseconds timeout) {
    if (max_events > events_buffer_size) {
        // todo: dynamically change the buffer size
        throw illegal_argument_exception();
    }

    auto events = reinterpret_cast<epoll_event*>(m_events);
    const auto count = ::epoll_wait(get_descriptor(), events, static_cast<int>(max_events), static_cast<int>(timeout.count()));
    if (count < 0) {
        int error = errno;
        if (error == EINTR) {
            // timeout has occurred
            m_data.set_count(0);
            return {&m_data};
        } else {
            throw io_exception(error);
        }
    }

    m_data.set_count(count);
    return {&m_data};
}

void resource_poller::handle_error() {
    int error = errno;
    throw io_exception(error);
}

resource_poller::event_data::event_data(void* events)
    : m_events(events)
    , m_count(0)
{}

size_t resource_poller::event_data::count() const {
    return m_count;
}

void resource_poller::event_data::set_count(size_t count) {
    m_count = count;
}

descriptor resource_poller::event_data::get_descriptor(size_t index) const {
    const auto events = reinterpret_cast<epoll_event*>(m_events);
    return static_cast<descriptor>(events[index].data.fd);
}

events::event_types resource_poller::event_data::get_events(size_t index) const {
    const auto events = reinterpret_cast<epoll_event*>(m_events);
    return native_to_events(events[index].events);
}

}
