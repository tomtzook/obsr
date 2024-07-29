#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <cstring>

#include "internal_except.h"
#include "socket.h"


namespace obsr::os {

struct {
    int level;
    int opt;
} sockopt_natives[] = {
        {SOL_SOCKET, SO_REUSEPORT}
};

base_socket::base_socket()
    : resource(open_socket()) {
}

base_socket::base_socket(int fd)
    : resource(fd) {
}

void base_socket::setoption(sockopt_type opt, void* value, size_t size) {
    throw_if_closed();

    const auto sockopt = sockopt_natives[static_cast<size_t>(opt)];

    if (::setsockopt(fd(), sockopt.level, sockopt.opt, value, size)) {
        handle_error();
    }
}

void base_socket::bind(const std::string& ip, uint16_t port) {
    throw_if_closed();

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (::bind(fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        handle_error();
    }
}

void base_socket::bind(uint16_t port) {
    throw_if_closed();

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        handle_error();
    }
}

void base_socket::handle_error() {
    const auto error_code = errno;

    if (error_code == ECONNRESET) { // TODO: maybe close not needed
        close();
    }

    throw io_exception(error_code);
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
    if (::listen(fd(), static_cast<int>(backlog_size))) {
        handle_error();
    }
}

std::unique_ptr<socket> server_socket::accept() {
    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);

    const auto new_fd = ::accept(fd(), reinterpret_cast<sockaddr*>(&addr), &addr_len);
    if (new_fd < 0) {
        handle_error();
    }

    return std::make_unique<socket>(new_fd);
}

socket::socket()
    : base_socket()
{}

socket::socket(int fd)
    : base_socket(fd)
{}

void socket::connect(const std::string& ip, uint16_t port) {
    throw_if_closed();

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (::connect(fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        handle_error();
    }
}

size_t socket::read(uint8_t* buffer, size_t buffer_size) {
    throw_if_closed();

    const auto result = ::read(fd(), buffer, buffer_size);
    if (result == 0) {
        throw eof_exception(); // TODO: MAYBE NOT TREAT THIS AS EXCEPTION
    } else if (result < 0) {
        handle_error();
    }

    return result;
}

size_t socket::write(const uint8_t* buffer, size_t size) {
    throw_if_closed();

    const auto result = ::write(fd(), buffer, size);
    if (result < 0) {
        handle_error();
    }

    return result;
}

}
