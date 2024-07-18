#pragma once

#include <memory>

#include "obsr_types.h"
#include "obsr_except.h"

namespace obsr {

template<typename type_, size_t capacity_>
class handle_table {
public:
    handle_table();

    const type_* operator[](handle handle) const;
    type_* operator[](handle handle);

    bool has(handle handle) const;

    template<typename... arg_>
    handle allocate_new(arg_&&... args);
    std::unique_ptr<type_> release(handle handle);

private:
    size_t find_next_available_spot() const;

    std::unique_ptr<type_> m_data[capacity_];
};

template<typename type_, size_t capacity_>
handle_table<type_, capacity_>::handle_table() {
}

template<typename type_, size_t capacity_>
const type_* handle_table<type_, capacity_>::operator[](handle handle) const {
    if (!has(handle)) {
        throw no_such_handle_exception(handle);
    }

    auto index = static_cast<size_t>(handle);
    return m_data[index].get();
}

template<typename type_, size_t capacity_>
type_* handle_table<type_, capacity_>::operator[](handle handle) {
    if (!has(handle)) {
        throw no_such_handle_exception(handle);
    }

    auto index = static_cast<size_t>(handle);
    return m_data[index].get();
}

template<typename type_, size_t capacity_>
bool handle_table<type_, capacity_>::has(handle handle) const {
    if (handle == empty_handle) {
        return false;
    }

    auto index = static_cast<size_t>(handle);
    if (m_data[index]) {
        return true;
    } else {
        return false;
    }
}

template<typename type_, size_t capacity_>
template<typename... arg_>
handle handle_table<type_, capacity_>::allocate_new(arg_&&... args) {
    auto index = find_next_available_spot();
    m_data[index] = std::make_unique<type_>(args...);

    return static_cast<handle>(index);
}

template<typename type_, size_t capacity_>
std::unique_ptr<type_> handle_table<type_, capacity_>::release(handle handle) {
    if (!has(handle)) {
        throw no_such_handle_exception(handle);
    }

    auto index = static_cast<size_t>(handle);

    std::unique_ptr<type_> data;
    m_data[index].swap(data);

    return std::move(data);
}

template<typename type_, size_t capacity_>
size_t handle_table<type_, capacity_>::find_next_available_spot() const {
    for (int i = 0; i < capacity_; ++i) {
        if (!m_data[i]) {
            return i;
        }
    }

    throw no_space_exception();
}

}
