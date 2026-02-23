#pragma once

#include <cstring>
#include <atomic>
#include <memory>

namespace obsr::diagnostics {

template<typename t_>
struct rw_double_buffer {
    using type = t_;

    rw_double_buffer();

    std::shared_ptr<const type> read() const;
    type& write();
    void swap();

private:
    std::atomic<std::shared_ptr<const type>> m_read_data;
    type m_write_data;
};

template<typename t_, size_t size_>
struct non_blocking_queue {
    non_blocking_queue() = default;

    void copy_into(std::array<t_, size_>&, uint64_t& read_index, uint64_t& write_index);

    void push(t_&& t);
    std::optional<t_> pop();

private:
    std::atomic_uint64_t m_read_idx = 0;
    std::atomic_uint64_t m_write_idx = 0;
    std::array<t_, size_> m_data{};
};

template<typename t_, size_t size_>
struct non_blocking_circular_buffer {
    non_blocking_circular_buffer() = default;

    void copy_into(std::array<t_, size_>&, uint64_t& write_index, size_t& count);

    void add(t_&& t);

private:
    std::atomic_uint64_t m_write_idx = 0;
    std::atomic_bool m_full = false;
    std::array<t_, size_> m_data{};
};

template<typename t_>
rw_double_buffer<t_>::rw_double_buffer()
    : m_read_data(std::make_shared<const type>())
    , m_write_data()
{}

template<typename t_>
std::shared_ptr<const t_> rw_double_buffer<t_>::read() const {
    return m_read_data.load();
}

template<typename t_>
t_& rw_double_buffer<t_>::write() {
    return m_write_data;
}

template<typename t_>
void rw_double_buffer<t_>::swap() {
    // create copy to update the read data
    auto new_data = std::make_shared<const type>(m_write_data);
    m_read_data.store(std::move(new_data));
}

template<typename t_, size_t size_>
void non_blocking_queue<t_, size_>::copy_into(std::array<t_, size_>& target, uint64_t& read_index, uint64_t& write_index) {
    memcpy(target.data(), m_data.data(), size_ * sizeof(t_));

    read_index = m_read_idx.load(std::memory_order_relaxed);
    write_index = m_write_idx.load(std::memory_order_relaxed);
}

template<typename t_, size_t size_>
void non_blocking_queue<t_, size_>::push(t_&& t) {
    uint64_t write_index;
    do {
        auto index = m_write_idx.load(std::memory_order_relaxed);
        write_index = (index + 1) % size_;

        // need to make sure we didn't reach the reader
        if (write_index == m_read_idx.load(std::memory_order_acquire)) {
            // no room
            return;
        }

        if (m_write_idx.compare_exchange_weak(index, write_index)) {
            // acquired a spot
            break;
        }
    } while (true);

    m_data[write_index] = std::move(t); // TODO: MAKE SURE WE ARE NOT ACCESSED BEFORE THIS IS READY
}

template<typename t_, size_t size_>
std::optional<t_> non_blocking_queue<t_, size_>::pop() {
    uint64_t read_index;
    do {
        auto index = m_read_idx.load(std::memory_order_relaxed);
        // need to make sure we didn't reach the reader
        if (index == m_write_idx.load(std::memory_order_acquire)) {
            // no room
            return std::nullopt;
        }

        read_index = (index + 1) % size_;
        if (m_read_idx.compare_exchange_weak(index, read_index)) {
            // acquired a spot
            break;
        }
    } while (true);

    return std::move(m_data[read_index]);
}

template<typename t_, size_t size_>
void non_blocking_circular_buffer<t_, size_>::copy_into(std::array<t_, size_>& target, uint64_t& write_index, size_t& count) {
    memcpy(target.data(), m_data.data(), size_ * sizeof(t_));

    write_index = m_write_idx.load(std::memory_order_relaxed);
    const auto full = m_full.load(std::memory_order_relaxed);
    count = full ? size_ : write_index;
}

template<typename t_, size_t size_>
void non_blocking_circular_buffer<t_, size_>::add(t_&& t) {
    uint64_t write_index;
    do {
        auto index = m_write_idx.load(std::memory_order_relaxed);
        write_index = (index + 1) % size_;

        if (m_write_idx.compare_exchange_weak(index, write_index)) {
            // acquired a spot
            break;
        }
    } while (true);

    m_data[write_index] = std::move(t);

    if (write_index == 0) {
        m_full.store(true);
    }
}

}
