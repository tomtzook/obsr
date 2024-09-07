
#include "serialize.h"


namespace obsr::io {

// todo: are floating point bits storage the same across arches?

template<typename t_>
std::optional<t_> read(readable_buffer* buf) {
    t_ value;
    bool res = buf->read(reinterpret_cast<uint8_t*>(&value), sizeof(t_));
    if (!res) {
        return {};
    }

    return std::move(value);
}

template<typename t_>
bool write(writable_buffer* buf, const t_& value) {
    return buf->write(reinterpret_cast<const uint8_t*>(&value), sizeof(t_));
}

deserializer::deserializer(readable_buffer* buffer)
    : m_buffer(buffer)
{}

std::optional<uint8_t> deserializer::read8() {
    return read<uint8_t>(m_buffer);
}

std::optional<uint16_t> deserializer::read16() {
    return read<uint16_t>(m_buffer);
}

std::optional<uint32_t> deserializer::read32() {
    return read<uint32_t>(m_buffer);
}

std::optional<uint64_t> deserializer::read64() {
    return read<uint64_t>(m_buffer);
}

std::optional<float> deserializer::readf32() {
    return read<float>(m_buffer);
}

std::optional<double> deserializer::readf64() {
    return read<double>(m_buffer);
}

std::optional<size_t> deserializer::read_size() {
    size_t value = 0;
    uint64_t shift = 0;

    while (true) {
        const auto byte_opt = read8();
        if (!byte_opt) {
            return {};
        }

        const auto byte = byte_opt.value();
        value |= (byte & 0x7f) << shift;
        if (!(byte & 0x80)) {
            break;
        }

        shift += 7;
    }

    return value;
}

std::optional<std::span<uint8_t>> deserializer::read_raw() {
    const auto size_opt = read_size();
    if (!size_opt) {
        return {};
    }

    const auto size = size_opt.value();
    expand_buffer(size);

    if (!m_buffer->read(m_data.get(), size)) {
        return {};
    }

    return {{m_data.get(), size}};
}

std::optional<std::string_view> deserializer::read_str() {
    const auto value_opt = read_raw();
    if (!value_opt) {
        return {};
    }

    const auto value = value_opt.value();
    static_assert(sizeof(char) == sizeof(uint8_t), "char is uint8");
    return {{reinterpret_cast<const char*>(value.data()), value.size()}};
}

std::optional<obsr::value> deserializer::read_value(value_type type) {
    switch (type) {
        case value_type::raw: {
            const auto value_opt = read_raw();
            if (!value_opt) {
                return {};
            }

            return value::make_raw(value_opt.value());
        }
        case value_type::boolean: {
            const auto value_opt = read8();
            if (!value_opt) {
                return {};
            }

            return value::make_boolean(static_cast<bool>(value_opt.value()));
        }
        case value_type::integer32: {
            const auto value_opt = read32();
            if (!value_opt) {
                return {};
            }

            return value::make_int32(static_cast<int32_t>(value_opt.value()));
        }
        case value_type::integer64: {
            const auto value_opt = read64();
            if (!value_opt) {
                return {};
            }

            return value::make_int64(static_cast<int64_t>(value_opt.value()));
        }
        case value_type::floating_point32: {
            const auto value_opt = readf32();
            if (!value_opt) {
                return {};
            }

            return value::make_float(static_cast<float>(value_opt.value()));
        }
        case value_type::floating_point64: {
            const auto value_opt = readf64();
            if (!value_opt) {
                return {};
            }

            return value::make_double(static_cast<double>(value_opt.value()));
        }
        case value_type::integer32_array: {
            const auto value_opt = read_arr<int32_t>();
            if (!value_opt) {
                return {};
            }

            const auto value = value_opt.value();
            return value::make_int32_array(value);
        }
        case value_type::integer64_array: {
            const auto value_opt = read_arr<int64_t>();
            if (!value_opt) {
                return {};
            }

            const auto value = value_opt.value();
            return value::make_int64_array(value);
        }
        case value_type::floating_point32_array: {
            const auto value_opt = read_arr<float>();
            if (!value_opt) {
                return {};
            }

            const auto value = value_opt.value();
            return value::make_float_array(value);
        }
        case value_type::floating_point64_array: {
            const auto value_opt = read_arr<double>();
            if (!value_opt) {
                return {};
            }

            const auto value = value_opt.value();
            return value::make_double_array(value);
        }
        case value_type::empty:
            return value::make();
        default:
            return {};
    }
}

void deserializer::expand_buffer(size_t size) {
    if (m_data && m_data_size >= size) {
        return;
    }

    m_data.reset(new uint8_t[size]);
}

serializer::serializer(writable_buffer* buffer)
    : m_buffer(buffer)
{}

bool serializer::write8(uint8_t value) {
    return write(m_buffer, value);
}

bool serializer::write16(uint16_t value) {
    return write(m_buffer, value);
}

bool serializer::write32(uint32_t value) {
    return write(m_buffer, value);
}

bool serializer::write64(uint64_t value) {
    return write(m_buffer, value);
}

bool serializer::writef32(float value) {
    return write(m_buffer, value);
}

bool serializer::writef64(double value) {
    return write(m_buffer, value);
}

bool serializer::write_size(size_t value) {
    do {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;
        }

        if (!write8(byte)) {
            return false;
        }
    } while (value != 0);

    return true;
}

bool serializer::write_raw(const uint8_t* ptr, size_t size) {
    if (!write_size(size)) {
        return false;
    }

    return m_buffer->write(ptr, size);
}

bool serializer::write_str(std::string_view str) {
    return write_raw(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

bool serializer::write_value(const value& value) {
    switch (value.get_type()) {
        case value_type::raw: {
            auto arr = value.get_raw();
            return write_raw(arr.data(), arr.size_bytes());
        }
        case value_type::boolean: {
            return write8(value.get_boolean());
        }
        case value_type::integer32: {
            return write32(value.get_int32());
        }
        case value_type::integer64: {
            return write64(value.get_int64());
        }
        case value_type::floating_point32: {
            return writef32(value.get_float());
        }
        case value_type::floating_point64: {
            return writef64(value.get_double());
        }
        case value_type::integer32_array: {
            auto arr = value.get_int32_array();
            return write_arr(arr);
        }
        case value_type::integer64_array: {
            auto arr = value.get_int64_array();
            return write_arr(arr);
        }
        case value_type::floating_point32_array: {
            auto arr = value.get_float_array();
            return write_arr(arr);
        }
        case value_type::floating_point64_array: {
            auto arr = value.get_double_array();
            return write_arr(arr);
        }
        case value_type::empty:
            return true;
        default:
            return false;
    }
}

}
