#pragma once

#include <algorithm>
#include <cstdint>

namespace obsr::io {

class readable_buffer {
public:
    virtual ~readable_buffer() = default;
    virtual bool read(uint8_t* buffer, size_t size) = 0;
};

class writable_buffer {
public:
    virtual ~writable_buffer() = default;
    virtual bool write(const uint8_t* buffer, size_t size) = 0;
};

class readonly_buffer_view final : public readable_buffer {
public:
    readonly_buffer_view();

    void reset(const uint8_t* buffer, size_t size);

    bool read(uint8_t* buffer, size_t size) override;

private:
    const uint8_t* m_buffer;
    size_t m_read_pos;
    size_t m_size;
};

class linear_buffer final : public writable_buffer {
public:
    explicit linear_buffer(size_t size);
    ~linear_buffer() override;

    [[nodiscard]] const uint8_t* data() const;
    [[nodiscard]] size_t pos() const;
    [[nodiscard]] size_t size() const;

    [[nodiscard]] bool can_write(size_t size) const;

    void reset();
    bool write(const uint8_t* buffer, size_t size) override;

private:
    uint8_t* m_buffer;
    size_t m_write_pos;
    size_t m_size;
};

class circular_buffer final : public readable_buffer, public writable_buffer {
public:
    explicit circular_buffer(size_t size);
    ~circular_buffer() override;

    [[nodiscard]] size_t read_available() const;
    [[nodiscard]] size_t write_available() const;

    [[nodiscard]] bool can_read(size_t size) const;
    [[nodiscard]] bool can_write(size_t size) const;

    void reset();

    bool find_and_seek_read(uint8_t byte);
    void seek_read(size_t offset);

    bool read(uint8_t* buffer, size_t size) override;
    bool write(const uint8_t* buffer, size_t size) override;

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
