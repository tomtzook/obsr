#pragma once

#include <exception>
#include <utility>

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

class no_parent_exception : public exception {
public:
    [[nodiscard]] const char* what() const noexcept override {
        return "no parent for object as it is root";
    }
};

class invalid_path_exception : public exception {
public:
    explicit invalid_path_exception(std::string  path)
        : m_path(std::move(path))
    {}
    explicit invalid_path_exception(std::string_view path)
        : m_path(path)
    {}

    [[nodiscard]] const std::string& get_path() const {
        return m_path;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return "requested path is badly formatted";
    }

private:
    std::string m_path;
};

class invalid_name_exception : public exception {
public:
    explicit invalid_name_exception(std::string  name)
        : m_name(std::move(name))
    {}
    explicit invalid_name_exception(std::string_view name)
        : m_name(name)
    {}

    [[nodiscard]] const std::string& get_name() const {
        return m_name;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return "requested name contains invalid parameters";
    }

private:
    std::string m_name;
};

class entry_does_not_exist_exception : public exception {
public:
    explicit entry_does_not_exist_exception(obsr::entry entry)
        : m_entry(entry)
    {}

    [[nodiscard]] obsr::entry get_entry() const {
        return m_entry;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return "entry does not exist";
    }

private:
    obsr::entry m_entry;
};

}
