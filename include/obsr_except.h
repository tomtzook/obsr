#pragma once

#include <exception>

#include "obsr_types.h"


namespace obsr {

class exception : public std::exception {

};

class invalid_handle_exception : public exception {
public:
    explicit invalid_handle_exception(handle handle);

private:
    handle m_handle;
};

class no_such_handle_exception : public exception {
public:
    explicit no_such_handle_exception(handle handle);

private:
    handle m_handle;
};

class no_space_exception : public exception {

};

class duplicate_exception : public exception {

};

class entry_type_mismatch_exception : public exception {
public:
    explicit entry_type_mismatch_exception(entry entry, value_type actual_type, value_type new_type);

private:
    entry m_entry;
    value_type m_actual_type;
    value_type m_new_type;
};

class cannot_delete_root_exception : public exception {

};

}
