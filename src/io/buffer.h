#pragma once

#include <cstdint>
#include <algorithm>
#include <functional>

namespace obsr::io {


template<typename t_>
concept readable = requires(t_ t, uint8_t* f1_buffer, size_t f1_size) {
    { t.read(f1_buffer, f1_size) } -> std::same_as<bool>;
};

template<typename t_>
concept writable = requires(t_ t, const uint8_t* f1_buffer, size_t f1_size) {
    { t.write(f1_buffer, f1_size) } -> std::same_as<bool>;
};

using read_func = std::function<bool(uint8_t*, size_t)>;
using write_func = std::function<bool(const uint8_t*, size_t)>;

template<readable t_>
read_func create_read_func(t_& t) {
    return std::bind_front(&t_::read, t);
}

template<writable t_>
write_func create_write_func(t_& t) {
    return std::bind_front(&t_::write, t);
}

class readonly_buffer_view final {
public:
    readonly_buffer_view();

    void reset(const uint8_t* buffer, size_t size);

    bool read(uint8_t* buffer, size_t size);

private:
    const uint8_t* m_buffer;
    size_t m_read_pos;
    size_t m_size;
};

class linear_buffer final {
public:
    explicit linear_buffer(size_t size);
    ~linear_buffer();

    [[nodiscard]] const uint8_t* data() const;
    [[nodiscard]] size_t pos() const;
    [[nodiscard]] size_t size() const;

    [[nodiscard]] bool can_write(size_t size) const;

    void reset();
    bool write(const uint8_t* buffer, size_t size);

private:
    uint8_t* m_buffer;
    size_t m_write_pos;
    size_t m_size;
};

class circular_buffer final {
public:
    explicit circular_buffer(size_t size);
    ~circular_buffer();

    [[nodiscard]] size_t read_available() const;
    [[nodiscard]] size_t write_available() const;

    [[nodiscard]] bool can_read(size_t size) const;
    [[nodiscard]] bool can_write(size_t size) const;

    void reset();

    bool find_and_seek_read(uint8_t byte);
    void seek_read(size_t offset);

    bool read(uint8_t* buffer, size_t size);
    bool write(const uint8_t* buffer, size_t size);

    template<typename t_>
    bool read(t_& t_out) {
        t_ t;
        if (!read(reinterpret_cast<uint8_t*>(&t), sizeof(t))) {
            return false;
        }

        t_out = std::move(t);
        return true;
    }

    template<typename t_>
    bool write(const t_& t) {
        return write(reinterpret_cast<const uint8_t*>(&t), sizeof(t));
    }

    template<typename t_>
    bool write(const t_* t, size_t size) {
        return write(reinterpret_cast<const uint8_t*>(t), size);
    }

private:
    uint8_t* m_buffer;
    size_t m_read_pos;
    size_t m_write_pos;
    size_t m_size;
};

}
