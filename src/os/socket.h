#pragma once

#include <string>
#include <memory>

#include "os/io.h"

namespace obsr::os {

// TODO: SUPPORT NON-BLOCKING
// TODO: BASE FLAGS WANTED FOR SOCKETS?

enum class sockopt_type : size_t {
    reuse_port
};

template<sockopt_type opt_, typename type_>
struct _sockopt {
    static constexpr auto opt = opt_;
    using type = type_;
};

template<typename t_>
struct _sockopt_hack : public std::false_type {};

#define define_sockopt(name, opt, type) \
    using sockopt_ ##name = _sockopt<opt, type>; \
    template<> struct _sockopt_hack<sockopt_ ##name> : public std::true_type {};

define_sockopt(reuseport, sockopt_type::reuse_port, bool);

class base_socket : public resource {
public:
    base_socket();
    explicit base_socket(int fd);

    void setoption(sockopt_type opt, void* value, size_t size);

    template<typename t_,
            typename std::enable_if<
                    _sockopt_hack<t_>::value,
                    bool>::type = 0
    >
    void setoption(typename t_::type value) {
        if constexpr (std::is_same_v<typename t_::type, bool>) {
            int _value = static_cast<int>(value);
            setoption(t_::opt, &_value, sizeof(_value));
        } else {
            setoption(t_::opt, &value, sizeof(typename t_::type));
        }
    }

    void configure_blocking(bool blocking);

    void bind(const std::string& ip, uint16_t port);
    void bind(uint16_t port);

protected:
    void handle_error();

private:
    static int open_socket();
    bool m_is_blocking;
};

class server_socket;
class socket;

class server_socket : public base_socket {
public:
    server_socket();

    void listen(size_t backlog_size);
    std::unique_ptr<socket> accept();
};

class socket : public base_socket, public readable, public writable {
public:
    socket();
    explicit socket(int fd);

    void connect(const std::string& ip, uint16_t port);

    size_t read(uint8_t* buffer, size_t buffer_size) override;
    size_t write(const uint8_t* buffer, size_t size) override;
};

}
