#pragma once

#include "obsr_except.h"

namespace obsr {

class io_exception : public exception {
public:
    explicit io_exception(int error_code)
        : m_error_code(error_code)
    {}

    [[nodiscard]] int get_code() const {
        return m_error_code;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return "io exception";
    }

private:
    int m_error_code;
};

class closed_fd_exception : public io_exception {
public:
    closed_fd_exception()
        : io_exception(0)
    {}

    [[nodiscard]] const char* what() const noexcept override {
        return "file descriptor is closed";
    }
};

class eof_exception : public io_exception {
public:
    eof_exception()
        : io_exception(0)
    {}

    [[nodiscard]] const char* what() const noexcept override {
        return "eof was reached";
    }
};

class illegal_state_exception : public exception {
public:
    explicit illegal_state_exception(const char* what)
        : m_what(what)
    {}

    [[nodiscard]] const char* what() const noexcept override {
        return m_what;
    }

private:
    const char* m_what;
};

class illegal_argument_exception : public exception {
public:
    explicit illegal_argument_exception(const char* what)
        : m_what(what)
    {}

    [[nodiscard]] const char* what() const noexcept override {
        return m_what;
    }

private:
    const char* m_what;
};

}
