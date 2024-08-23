#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <cstring>

#include "internal_except.h"
#include "socket.h"


namespace obsr::os {

struct {
    int level;
    int opt;
} sockopt_natives[] = {
        {SOL_SOCKET, SO_REUSEPORT},
        {SOL_SOCKET, SO_KEEPALIVE}
};

base_socket::base_socket()
    : resource(open_socket())
    , m_disabled(false)
    , m_is_blocking(true) {
    configure_blocking(true);
}

base_socket::base_socket(int fd)
    : resource(fd) {
}

void base_socket::setoption(sockopt_type opt, void* value, size_t size) {
    throw_if_closed();
    throw_if_disabled();

    const auto sockopt = sockopt_natives[static_cast<size_t>(opt)];

    if (::setsockopt(fd(), sockopt.level, sockopt.opt, value, size)) {
        handle_call_error();
    }
}

void base_socket::configure_blocking(bool blocking) {
    throw_if_disabled();

    auto fd = this->fd();
    auto flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        handle_call_error();
    }

    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    if (fcntl(fd, F_SETFL, flags)) {
        handle_call_error();
    }

    m_is_blocking = blocking;
}

void base_socket::bind(const std::string& ip, uint16_t port) {
    throw_if_closed();
    throw_if_disabled();

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (::bind(fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        handle_call_error();
    }
}

void base_socket::bind(uint16_t port) {
    throw_if_closed();
    throw_if_disabled();

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        handle_call_error();
    }
}

base_socket::error_code_t base_socket::get_call_error() const {
    return errno;
}

base_socket::error_code_t base_socket::get_internal_error() {
    int code;
    socklen_t len = sizeof(code);
    if (::getsockopt(fd(), SOL_SOCKET, SO_ERROR, &code, &len)) {
        handle_call_error();
    }

    return code;
}

void base_socket::handle_call_error(error_code_t code) {
    if (code == 0) {
        code = get_call_error();
    }

    if (code == ECONNRESET) {
        close();
    }

    throw io_exception(code);
}

void base_socket::check_internal_error(error_code_t code) {
    if (code == 0) {
        code = get_internal_error();
    }

    if (code != 0) {
        throw io_exception(code);
    }
}

void base_socket::throw_if_disabled() {
    if (m_disabled) {
        throw illegal_state_exception();
    }
}

int base_socket::open_socket() {
    int m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd < 0) {
        throw io_exception(errno);
    }

    return m_fd;
}

server_socket::server_socket()
    : base_socket()
{}

void server_socket::listen(size_t backlog_size) {
    throw_if_closed();
    throw_if_disabled();

    if (::listen(fd(), static_cast<int>(backlog_size))) {
        handle_call_error();
    }
}

std::unique_ptr<socket> server_socket::accept() {
    throw_if_closed();
    throw_if_disabled();

    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);

    const auto new_fd = ::accept(fd(), reinterpret_cast<sockaddr*>(&addr), &addr_len);
    if (new_fd < 0) {
        handle_call_error();
    }

    return std::make_unique<socket>(new_fd);
}

socket::socket()
    : base_socket()
    , m_waiting_connection(false)
{}

socket::socket(int fd)
    : base_socket(fd)
    , m_waiting_connection(false)
{}

void socket::connect(std::string_view ip, uint16_t port) {
    throw_if_closed();
    throw_if_disabled();

    std::string ip_c(ip);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, ip_c.c_str(), &addr.sin_addr);

    if (::connect(fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        const auto error_code = get_call_error();
        if (error_code == EINPROGRESS && !is_blocking()) {
            // while in non-blocking mode, socket operations may return inprogress as a result
            // to operations they have not yet finished. this is fine.
            m_waiting_connection = true;
            disable();
        } else {
            handle_call_error(error_code);
        }
    }
}

void socket::finalize_connect() {
    throw_if_closed();

    if (!m_waiting_connection) {
        return;
    }

    m_waiting_connection = false;
    enable();

    // for non-blocking connect, we need to make sure it actually succeeded in the end.
    check_internal_error();
}

size_t socket::read(uint8_t* buffer, size_t buffer_size) {
    throw_if_closed();
    throw_if_disabled();

    const auto result = ::read(fd(), buffer, buffer_size);
    if (result == 0) {
        throw eof_exception();
    } else if (result < 0) {
        handle_call_error();
    }

    return result;
}

size_t socket::write(const uint8_t* buffer, size_t size) {
    throw_if_closed();
    throw_if_disabled();

    const auto result = ::write(fd(), buffer, size);
    if (result < 0) {
        handle_call_error();
    }

    return result;
}

}
