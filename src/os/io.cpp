
#include <unistd.h>
#include <poll.h>

#include "internal_except.h"
#include "util/time.h"
#include "io.h"

namespace obsr::os {

static short events_to_native(uint32_t events) {
    short r_events = 0;
    if ((events & selector::poll_in) != 0) {
        r_events |= POLLIN;
    }
    if ((events & selector::poll_in_urgent) != 0) {
        r_events |= POLLPRI;
    }
    if ((events & selector::poll_out) != 0) {
        r_events |= POLLOUT;
    }
    if ((events & selector::poll_error) != 0) {
        r_events |= POLLERR;
    }
    if ((events & selector::poll_hung) != 0) {
        r_events |= POLLHUP;
    }

    return r_events;
}

static uint32_t native_to_events(short events) {
    uint32_t r_events = 0;
    if ((events & POLLIN) != 0) {
        r_events |= selector::poll_in;
    }
    if ((events & POLLPRI) != 0) {
        r_events |= selector::poll_in_urgent;
    }
    if ((events & POLLOUT) != 0) {
        r_events |= selector::poll_out;
    }
    if ((events & POLLERR) != 0) {
        r_events |= selector::poll_error;
    }
    if ((events & POLLHUP) != 0) {
        r_events |= selector::poll_hung;
    }

    return r_events;
}

resource::resource(int fd)
    : m_fd(fd)
{}

resource::~resource() {
    close();
}

void resource::close() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

void resource::throw_if_closed() const {
    if (m_fd < 0) {
        throw closed_fd_exception();
    }
}

selector::poll_result::poll_result()
    : m_states()
    , m_last_update_time(0) {
}

bool selector::poll_result::has(obsr::handle handle) const {
    return has(handle, static_cast<uint32_t>(-1));
}

bool selector::poll_result::has(obsr::handle handle, poll_type type) const {
    return has(handle, static_cast<uint32_t>(type));
}

bool selector::poll_result::has(obsr::handle handle, uint32_t flags) const {
    const auto actual_flags = get(handle);
    return (actual_flags & flags) != 0;
}

uint32_t selector::poll_result::get(obsr::handle handle) const {
    const auto index = static_cast<size_t>(handle);
    if (index >= max_resources) {
        return 0;
    }

    const auto& state = m_states[index];
    if (state.handle != handle) {
        return 0;
    }

    if (state.update_time < m_last_update_time) {
        return 0;
    }

    return state.flags;
}

selector::selector()
    : m_handles()
    , m_native_data(nullptr) {
    m_native_data = new pollfd[max_resources];
    initialize_native_data();
}

selector::~selector() {
    delete[] reinterpret_cast<pollfd*>(m_native_data);
}

handle selector::add(std::shared_ptr<resource> resource, uint32_t flags) {
    const auto index = find_empty_resource_index();
    if (index < 0) {
        throw no_space_exception();
    }

    const auto fd = resource->fd();
    auto handle = m_handles.allocate_new(resource);
    auto handle_data = m_handles[handle];

    auto fds = reinterpret_cast<pollfd*>(m_native_data);
    auto& fd_data = fds[index];
    fd_data.fd = fd;
    fd_data.events = events_to_native(flags);
    handle_data->r_index = index;

    return handle;
}

std::shared_ptr<resource> selector::remove(obsr::handle handle) {
    auto handle_data = m_handles.release(handle);

    auto fds = reinterpret_cast<pollfd*>(m_native_data);
    auto& fd_data = fds[handle_data->r_index];
    fd_data.fd = -1;
    fd_data.events = 0;

    return handle_data->r_resource;
}

void selector::poll(poll_result& result, std::chrono::milliseconds timeout) {
    // note: if all fds are empty, then we will simply wait until timeout
    auto fds = reinterpret_cast<pollfd*>(m_native_data);
    if (::poll(fds, max_resources, static_cast<int>(timeout.count())) < 0) {
        throw io_exception(errno);
    }

    const auto now = time_now();
    result.m_last_update_time = now;

    for (auto [handle, data] : m_handles) {
        auto& fd_struct = fds[data.r_index];
        auto& state = result.m_states[handle];

        if (fd_struct.fd >= 0 && fd_struct.revents != 0) {
            const auto flags = native_to_events(fd_struct.revents);
            state.flags = flags;
            state.update_time = now;
            state.handle = handle;
        }
    }
}

ssize_t selector::find_empty_resource_index() {
    auto fds = reinterpret_cast<pollfd*>(m_native_data);
    for (int i = 0; i < max_resources; i++) {
        auto& fd_data = fds[i];
        if (fd_data.fd < 0) {
            return i;
        }
    }

    return -1;
}

void selector::initialize_native_data() {
    auto fds = reinterpret_cast<pollfd*>(m_native_data);
    for (int i = 0; i < max_resources; i++) {
        auto& fd_data = fds[i];
        fd_data.fd = -1;
    }
}

}
