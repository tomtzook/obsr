
#include <cstring>

#include "buffer.h"


namespace obsr::io {

readonly_buffer::readonly_buffer()
    : m_buffer(nullptr)
    , m_read_pos(0)
    , m_size(0)
{}

void readonly_buffer::reset(const uint8_t* buffer, size_t size) {
    m_buffer = buffer;
    m_read_pos = 0;
    m_size = size;
}

bool readonly_buffer::read(uint8_t* buffer, size_t size) {
    const auto space_to_max = (m_size - m_read_pos);
    if (size > space_to_max) {
        return false;
    }

    memcpy(buffer, m_buffer + m_read_pos, size);
    m_read_pos += size;

    return true;
}


buffer::buffer(size_t size)
    : m_buffer(new uint8_t[size])
    , m_read_pos(0)
    , m_write_pos(0)
    , m_size(size) {

}

// todo: there are several functions handling the indecies, combine

buffer::~buffer() {
    delete[] m_buffer;
}

size_t buffer::read_available() const {
    if (m_write_pos < m_read_pos) {
        return (m_size - m_read_pos) + m_write_pos;
    } else {
        return m_write_pos - m_read_pos;
    }
}

size_t buffer::write_available() const {
    if (m_write_pos >= m_read_pos) {
        return (m_size - m_write_pos) + m_write_pos;
    } else {
        return m_read_pos - m_write_pos;
    }
}

bool buffer::can_read(size_t size) const {
    const auto available = read_available();
    return available >= size;
}

bool buffer::can_write(size_t size) const {
    const auto available = write_available();
    return available >= size;
}

void buffer::reset() {
    m_read_pos = 0;
    m_write_pos = 0;
}

void buffer::seek_read(size_t offset) {
    if (m_write_pos < m_read_pos) {
        const auto space_to_max = (m_size - m_read_pos);
        if (offset > space_to_max && offset > space_to_max + m_write_pos) {
            offset = space_to_max + m_write_pos;
        }
    } else {
        const auto space = (m_write_pos - m_read_pos);
        if (offset > space) {
            offset = space;
        }
    }

    m_read_pos += offset;
    m_read_pos %= m_size;
}

bool buffer::read(uint8_t* buffer, size_t size) {
    if (m_write_pos < m_read_pos) {
        const auto space_to_max = (m_size - m_read_pos);
        if (size < space_to_max) {
            memcpy(buffer, m_buffer + m_read_pos, size);
        } else {
            if (size > space_to_max + m_write_pos) {
                return false;
            }

            memcpy(buffer, m_buffer + m_read_pos, size);
            memcpy(buffer + space_to_max, m_buffer, size - space_to_max);
        }
    } else {
        const auto space = (m_write_pos - m_read_pos);
        if (size > space) {
            return false;
        }

        memcpy(buffer, m_buffer + m_read_pos, size);
    }

    m_read_pos += size;
    m_read_pos %= m_size;

    return true;
}

bool buffer::write(const uint8_t* buffer, size_t size) {
    if (m_write_pos >= m_read_pos) {
        const auto space_to_max = (m_size - m_write_pos);
        if (size < space_to_max) {
            memcpy(m_buffer + m_write_pos, buffer, size);
        } else {
            if (size > space_to_max + m_read_pos) {
                return false;
            }

            memcpy(m_buffer + m_write_pos, buffer, space_to_max);
            memcpy(m_buffer, buffer + space_to_max, size - space_to_max);
        }
    } else {
        const auto space = (m_read_pos - m_write_pos);
        if (size > space) {
            return false;
        }

        memcpy(m_buffer + m_write_pos, buffer, size);
    }

    m_write_pos += size;
    m_write_pos %= m_size;

    return true;
}

void buffer::read_from(obsr::os::readable& readable) {
    if (m_write_pos >= m_read_pos) {
        auto space = (m_size - m_write_pos);
        auto read = readable.read(m_buffer + m_write_pos, space);
        if (read < space) {
            m_write_pos += read;
            return;
        }

        space = (m_read_pos - m_write_pos);
        read = readable.read(m_buffer, space);
        m_write_pos = read;
    } else {
        const auto space = (m_read_pos - m_write_pos);
        auto read = readable.read(m_buffer + m_write_pos, space);
        m_write_pos += read;
    }
}

bool buffer::write_into(obsr::os::writable& writable) {
    if (m_write_pos < m_read_pos) {
        auto space = (m_size - m_read_pos);
        if (space < 1) {
            return false;
        }

        auto written = writable.write(m_buffer + m_read_pos, space);
        if (written < space) {
            m_read_pos += written;
            return true;
        }

        space = m_read_pos;
        written = writable.write(m_buffer, space);
        m_read_pos = written;
    } else {
        const auto space = (m_write_pos - m_read_pos);
        if (space < 1) {
            return false;
        }

        auto written = writable.write(m_buffer + m_read_pos, space);
        m_read_pos += written;
    }

    return true;
}

}
