#pragma once

#include "obsr_except.h"

namespace obsr {

class io_exception : public exception {
public:
    explicit io_exception(int error_code)
        : m_error_code(error_code)
    {}

    int get_code() const {
        return m_error_code;
    }

private:
    int m_error_code;
};

class closed_fd_exception : public io_exception {
public:
    closed_fd_exception()
        : io_exception(-1)
    {}
};

class eof_exception : public io_exception {
public:
    eof_exception()
        : io_exception(-1)
    {}
};

}
