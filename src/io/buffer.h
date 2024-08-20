#pragma once

#include <cstdint>
#include <cstddef>

#include <optional>

#include "os/io.h"

namespace obsr::io {

class readable_buffer {
public:
    virtual bool read(uint8_t* buffer, size_t size) = 0;
};

class writable_buffer {
public:
    virtual bool write(const uint8_t* buffer, size_t size) = 0;
};

class readonly_buffer : public readable_buffer {
public:
    readonly_buffer();

    void reset(const uint8_t* buffer, size_t size);

    bool read(uint8_t* buffer, size_t size) override;

private:
    const uint8_t* m_buffer;
    size_t m_read_pos;
    size_t m_size;
};

class linear_buffer : public writable_buffer {
public:
    linear_buffer();

    const uint8_t* data() const;
    size_t pos() const;
    size_t size() const;

    void reset();
    bool write(const uint8_t* buffer, size_t size) override;

private:
    uint8_t m_buffer[1024]; // todo: find something better for this
    size_t m_write_pos;
    size_t m_size;
};

class buffer : public readable_buffer, public writable_buffer {
public:
    buffer(size_t size);
    ~buffer();

    size_t read_available() const;
    size_t write_available() const;

    bool can_read(size_t size) const;
    bool can_write(size_t size) const;

    void reset();

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

    void read_from(obsr::os::readable& readable);
    inline void read_from(obsr::os::readable* readable) {
        read_from(*readable);
    }

    bool write_into(obsr::os::writable& writable);
    inline bool write_into(obsr::os::writable* writable) {
        return write_into(*writable);
    }

private:
    uint8_t* m_buffer;
    size_t m_read_pos;
    size_t m_write_pos;
    size_t m_size;
};

}
