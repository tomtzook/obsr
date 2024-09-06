
#include "serialize.h"


namespace obsr::io {

// todo: are floating point bits storage the same across arches?

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

std::optional<obsr::value> readvalue(readable_buffer& buf, value_type type) {
    switch (type) {
        case value_type::boolean: {
            uint8_t value;
            if (!read8(buf, value)) {
                return {};
            }

            return value::make_boolean(static_cast<bool>(value));
        }
        case value_type::integer32: {
            uint32_t value;
            if (!read32(buf, value)) {
                return {};
            }

            return value::make_int32(static_cast<int32_t>(value));
        }
        case value_type::integer64: {
            uint64_t value;
            if (!read64(buf, value)) {
                return {};
            }

            return value::make_int64(static_cast<int64_t>(value));
        }
        case value_type::floating_point32: {
            float value;
            if (!readf32(buf, value)) {
                return {};
            }

            return value::make_float(value);
        }
        case value_type::floating_point64: {
            double value;
            if (!readf64(buf, value)) {
                return {};
            }

            return value::make_double(value);
        }
        case value_type::integer32_array: {
            uint8_t temp_buffer[256];
            size_t size = sizeof(temp_buffer);
            if (!readraw(buf, temp_buffer, size)) {
                return {};
            }

            std::span<const int32_t> span{reinterpret_cast<int32_t*>(temp_buffer),
                                          size / sizeof(int32_t)};
            return value::make_int32_array(span);
        }
        case value_type::empty:
            return value::make();
        default:
            return {};
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
    // todo: improve as to not writevalue the full 64bit always
    if (!write64(buf, size)) {
        return false;
    }

    return buf.write(ptr, size);
}

bool writevalue(writable_buffer& buf, const value& value) {
    switch (value.get_type()) {
        case value_type::empty:
            return true;
        case value_type::boolean:
            return write8(buf, value.get_boolean());
        case value_type::integer32:
            return write32(buf, value.get_int32());
        case value_type::integer64:
            return write64(buf, value.get_int64());
        case value_type::floating_point32:
            return writef32(buf, value.get_float());
        case value_type::floating_point64:
            return writef64(buf, value.get_double());
        case value_type::integer32_array: {
            auto arr = value.get_int32_array();
            return writeraw(buf, reinterpret_cast<const uint8_t*>(arr.data()), arr.size_bytes());
        }
        default:
            return true;
    }
}

}
