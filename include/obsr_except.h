#pragma once

#include <exception>

#include "obsr_types.h"


namespace obsr {

class exception : public std::exception {
public:
    [[nodiscard]] const char* what() const noexcept override {
        return "obsr exception";
    }
};

class no_such_handle_exception : public exception {
public:
    explicit no_such_handle_exception(handle handle);

    [[nodiscard]] inline obsr::handle get_handle() const {
        return m_handle;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return "requested handle does not exist";
    }

private:
    obsr::handle m_handle;
};

class no_space_exception : public exception {
public:
    [[nodiscard]] const char* what() const noexcept override {
        return "no more space";
    }
};

class entry_type_mismatch_exception : public exception {
public:
    explicit entry_type_mismatch_exception(entry entry, value_type actual_type, value_type new_type);

    [[nodiscard]] inline obsr::entry get_entry() const {
        return m_entry;
    }

    [[nodiscard]] inline value_type get_actual_type() const {
        return m_actual_type;
    }

    [[nodiscard]] inline value_type get_new_type() const {
        return m_new_type;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return "changing the type of a non-empty entry is not possible";
    }

private:
    obsr::entry m_entry;
    value_type m_actual_type;
    value_type m_new_type;
};

class cannot_delete_root_exception : public exception {
public:
    [[nodiscard]] const char* what() const noexcept override {
        return "deleting root object is not possible";
    }
};

class data_exceeds_size_limits_exception : public exception {
public:
    [[nodiscard]] const char* what() const noexcept override {
        return "provided data exceeds size limits and cannot be used";
    }
};

}
