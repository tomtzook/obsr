
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

class linux_poll_resource : public selector::poll_resource {
public:
    linux_poll_resource(size_t index, pollfd& fd_data)
        : m_index(index)
        , m_pollfd(fd_data)
    {}
    ~linux_poll_resource() override = default;

    bool valid() const override {
        return m_pollfd.fd >= 0;
    }

    bool has_result() const override {
        return valid() && m_pollfd.revents != 0;
    }

    uint32_t result_flags() const override {
        if (!has_result()) {
            return 0;
        }

        return native_to_events(m_pollfd.revents);
    }

    uint32_t flags() const override {
        if (!valid()) {
            return 0;
        }

        return native_to_events(m_pollfd.events);
    }

    void flags(uint32_t flags) override {
        if (!valid()) {
            return;
        }

        m_pollfd.events = events_to_native(flags);
    }

private:
    size_t m_index;
    pollfd& m_pollfd;

    friend class selector;
};

selector::selector()
    : m_native_data(nullptr)
    , m_resources() {
    m_native_data = new pollfd[max_resources];
    initialize_native_data();
}

selector::~selector() {
    delete[] reinterpret_cast<pollfd*>(m_native_data);

    for (int i = 0; i < max_resources; ++i) {
        delete m_resources[i];
    }
}

selector::poll_resource* selector::add(std::shared_ptr<resource> resource, uint32_t flags) {
    const auto index = find_empty_resource_index();
    if (index < 0) {
        throw no_space_exception();
    }

    const auto fd = resource->fd();

    auto fds = reinterpret_cast<pollfd*>(m_native_data);
    auto& fd_data = fds[index];
    fd_data.fd = fd;
    fd_data.events = events_to_native(flags);

    return m_resources[index];
}

void selector::remove(poll_resource* resource) {
    if (resource == nullptr) {
        return;
    }

    auto l_resource = reinterpret_cast<linux_poll_resource*>(resource);
    auto index = l_resource->m_index;

    auto fds = reinterpret_cast<pollfd*>(m_native_data);
    auto& fd_data = fds[index];
    fd_data.fd = -1;
    fd_data.events = 0;
}

void selector::poll(std::chrono::milliseconds timeout) {
    // note: if all fds are empty, then we will simply wait until timeout
    auto fds = reinterpret_cast<pollfd*>(m_native_data);
    if (::poll(fds, max_resources, static_cast<int>(timeout.count())) < 0) {
        throw io_exception(errno);
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

        m_resources[i] = new linux_poll_resource(i, fd_data);
    }
}

}
