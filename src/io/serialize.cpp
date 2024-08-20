
#include "serialize.h"


namespace obsr::io {

// todo: are floating point bits storage the same across arches?

static size_t calc_required_size(const value& value) {
    switch (value.type) {
        case value_type::boolean:
            return sizeof(uint8_t);
        case value_type::integer32:
            return sizeof(uint32_t);
        case value_type::integer64:
            return sizeof(uint64_t);
        case value_type::floating_point32:
            return sizeof(float);
        case value_type::floating_point64:
            return sizeof(double);
        case value_type::empty:
        default:
            return 0;
    }
}

template<typename t_>
bool read(readable_buffer& buf, t_& value_out) {
    t_ value;
    bool res = buf.read(reinterpret_cast<uint8_t*>(&value), sizeof(t_));
    if (!res) {
        return false;
    }

    value_out = std::move(value);
    return true;
}

template<typename t_>
bool write(writable_buffer& buf, const t_& value) {
    return buf.write(reinterpret_cast<const uint8_t*>(&value), sizeof(t_));
}

bool read8(readable_buffer& buf, uint8_t& value_out) {
    return read(buf, value_out);
}

bool read16(readable_buffer& buf, uint16_t& value_out) {
    return read(buf, value_out);
}

bool read32(readable_buffer& buf, uint32_t& value_out) {
    return read(buf, value_out);
}

bool readf32(readable_buffer& buf, float& value_out) {
    return read(buf, value_out);
}

bool read64(readable_buffer& buf, uint64_t& value_out) {
    return read(buf, value_out);
}

bool readf64(readable_buffer& buf, double& value_out) {
    return read(buf, value_out);
}

bool readraw(readable_buffer& buf, uint8_t* ptr, size_t& size) {
    size_t actual_size;
    if (!read64(buf, actual_size)) {
        // todo: problem, we cannot revert from this read if it fails,
        //  same for all subsequent reads here
        return false;
    }

    if (size < actual_size) {
        return false;
    }

    if (!buf.read(ptr, actual_size)) {
        return false;
    }

    size = actual_size;
    return true;
}

bool read(readable_buffer& buf, value_type type, value& value) {
    switch (type) {
        case value_type::boolean:
            return read8(buf, reinterpret_cast<uint8_t&>(value.value.boolean));
        case value_type::integer32:
            return read32(buf, reinterpret_cast<uint32_t &>(value.value.integer));
        case value_type::integer64:
            return read64(buf, reinterpret_cast<uint64_t&>(value.value.integer64));
        case value_type::floating_point32:
            return readf32(buf, value.value.floating_point);
        case value_type::floating_point64:
            return readf64(buf, value.value.floating_point64);
        case value_type::empty:
        default:
            return true;
    }
}

bool write8(writable_buffer& buf, uint8_t value) {
    return write(buf, value);
}

bool write16(writable_buffer& buf, uint16_t value) {
    return write(buf, value);
}

bool write32(writable_buffer& buf, uint32_t value) {
    return write(buf, value);
}

bool writef32(writable_buffer& buf, float value) {
    return write(buf, value);
}

bool write64(writable_buffer& buf, uint64_t value) {
    return write(buf, value);
}

bool writef64(writable_buffer& buf, double value) {
    return write(buf, value);
}

bool writeraw(writable_buffer& buf, const uint8_t* ptr, size_t size) {
    // todo: improve as to not write the full 64bit always
    if (!write64(buf, size)) {
        return false;
    }

    return buf.write(ptr, size);
}

bool write(writable_buffer& buf, value_type type, const value& value) {
    switch (type) {
        case value_type::empty:
            return true;
        case value_type::boolean:
            return write8(buf, value.value.boolean);
        case value_type::integer32:
            return write32(buf, value.value.integer);
        case value_type::integer64:
            return write64(buf, value.value.integer64);
        case value_type::floating_point32:
            return writef32(buf, value.value.floating_point);
        case value_type::floating_point64:
            return writef64(buf, value.value.floating_point64);
        default:
            return true;
    }
}

}
