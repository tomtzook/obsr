
#include "serialize.h"


namespace obsr::io {

static size_t calc_required_size(const value_t& value) {
    switch (value.type) {
        case value_type::raw:
            return value.value.raw.size;
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
    bool res = buf.read(value);
    if (!res) {
        return false;
    }

    value_out = std::move(value);
    return true;
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
    if (!buf.can_read(sizeof(uint64_t))) {
        return false;
    }

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

bool read(readable_buffer& buf, value_type type, value_t& value) {
    switch (type) {
        case value_type::raw: {
            // todo: who owns the raw value? do we receive it with a prepared buffer
            return readraw(buf, value.value.raw.ptr, value.value.raw.size);
        }
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

bool write8(buffer& buf, uint8_t value) {
    return buf.write(value);
}

bool write16(buffer& buf, uint16_t value) {
    return buf.write(value);
}

bool write32(buffer& buf, uint32_t value) {
    return buf.write(value);
}

bool writef32(buffer& buf, float value) {
    return buf.write(value);
}

bool write64(buffer& buf, uint64_t value) {
    return buf.write(value);
}

bool writef64(buffer& buf, double value) {
    return buf.write(value);
}

bool writestr(buffer& buf, const std::string_view& str) {
    // todo: improve as to not write the full 64bit always
    return writeraw(buf, reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

bool writeraw(buffer& buf, const uint8_t* ptr, size_t size) {
    // todo: improve as to not write the full 64bit always
    if (!buf.can_write(sizeof(uint64_t) + size)) {
        return false;
    }

    if (!write64(buf, size)) {
        return false;
    }

    return buf.write(ptr, size);
}

bool write(buffer& buf, const value_t& value) {
    const auto size = calc_required_size(value) + sizeof(uint8_t);
    if (!buf.can_write(size)) {
        return false;
    }

    const auto type = static_cast<uint8_t>(value.type);
    if (!write8(buf, type)) {
        return false;
    }

    switch (value.type) {
        case value_type::empty:
            return true;
        case value_type::raw:
            return writeraw(buf, value.value.raw.ptr, value.value.raw.size);
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
