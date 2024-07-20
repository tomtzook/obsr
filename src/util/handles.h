#pragma once

#include <memory>
#include <iterator>

#include "obsr_types.h"
#include "obsr_except.h"

namespace obsr {

template<typename type_, size_t capacity_>
class handle_table {
public:
    struct iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = type_;
        using pointer           = value_type*;
        using reference         = value_type&;

        iterator(std::unique_ptr<value_type>* ptr, size_t index)
            : m_ptr(ptr)
            , m_index(index) {

            if (!m_ptr[m_index] && m_index < capacity_) {
                iterate_to_next_element();
            }
        }

        std::pair<handle, reference> operator*() const {
            auto handle = static_cast<obsr::handle>(m_index);
            auto data = m_ptr[m_index].get();
            return {handle, *data};
        }

        iterator& operator++() {
            iterate_to_next_element();
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator== (const iterator& a, const iterator& b) {
            return a.m_index == b.m_index;
        }
        friend bool operator!= (const iterator& a, const iterator& b) {
            return a.m_index != b.m_index;
        }

    private:
        void iterate_to_next_element() {
            do {
                m_index++;
            } while (!m_ptr[m_index] && m_index < capacity_);
        }

        std::unique_ptr<value_type>* m_ptr;
        size_t m_index;
    };

    handle_table() = default;

    const type_* operator[](handle handle) const {
        if (!has(handle)) {
            throw no_such_handle_exception(handle);
        }

        auto index = static_cast<size_t>(handle);
        return m_data[index].get();
    }

    type_* operator[](handle handle) {
        if (!has(handle)) {
            throw no_such_handle_exception(handle);
        }

        auto index = static_cast<size_t>(handle);
        return m_data[index].get();
    }

    bool has(handle handle) const {
        if (handle == empty_handle) {
            return false;
        }

        auto index = static_cast<size_t>(handle);
        if (index >= capacity_) {
            return false;
        }

        if (m_data[index]) {
            return true;
        } else {
            return false;
        }
    }

    template<typename... arg_>
    handle allocate_new(arg_&&... args) {
        auto index = find_next_available_spot();
        auto handle = static_cast<obsr::handle>(index);

        m_data[index] = std::make_unique<type_>(args...);

        return handle;
    }

    template<typename... arg_>
    handle allocate_new_with_handle(arg_&&... args) {
        auto index = find_next_available_spot();
        auto handle = static_cast<obsr::handle>(index);

        m_data[index] = std::make_unique<type_>(handle, args...);

        return handle;
    }

    std::unique_ptr<type_> release(handle handle) {
        if (!has(handle)) {
            throw no_such_handle_exception(handle);
        }

        auto index = static_cast<size_t>(handle);

        std::unique_ptr<type_> data;
        m_data[index].swap(data);

        return std::move(data);
    }

    iterator begin() {
        return iterator(&m_data[0], 0);
    }
    iterator end()   {
        return iterator(&m_data[capacity_], capacity_);
    }

private:
    size_t find_next_available_spot() const {
        for (int i = 0; i < capacity_; ++i) {
            if (!m_data[i]) {
                return i;
            }
        }

        throw no_space_exception();
    }

    std::unique_ptr<type_> m_data[capacity_];
};

}
