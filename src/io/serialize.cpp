
#include "debug.h"
#include "util/bits.h"
#include "serialize.h"


namespace obsr::io {

#define LOG_MODULE "serialization"

static bool is_within_size_limits(size_t size) {
    if (size >= UINT8_MAX) {
        TRACE_ERROR(LOG_MODULE, "requested buffer/array too big: %lu", size);
        return false;
    }

    return true;
}

template<typename t_>
bool read(readable_buffer* buf, t_& value_out) {
    t_ value;
    bool res = buf->read(reinterpret_cast<uint8_t*>(&value), sizeof(t_));
    if (!res) {
        return false;
    }

    value_out = std::move(value);
    return true;
}

template<typename t_>
bool write(writable_buffer* buf, const t_& value) {
    return buf->write(reinterpret_cast<const uint8_t*>(&value), sizeof(t_));
}

deserializer::deserializer(readable_buffer* buffer)
    : m_buffer(buffer)
    , m_data()
    , m_data_size(0)
{}

std::optional<uint8_t> deserializer::read8() {
    uint8_t value;
    if (!read(m_buffer, value)) {
        return {};
    }

    return {value};
}

std::optional<uint16_t> deserializer::read16() {
    uint16_t value;
    if (!read(m_buffer, value)) {
        return {};
    }

    value = obsr::bits::host16(value);
    return {value};
}

std::optional<uint32_t> deserializer::read32() {
    uint32_t value;
    if (!read(m_buffer, value)) {
        return {};
    }

    value = obsr::bits::host32(value);
    return {value};
}

std::optional<uint64_t> deserializer::read64() {
    uint64_t value;
    if (!read(m_buffer, value)) {
        return {};
    }

    value = obsr::bits::host64(value);
    return {value};
}

std::optional<float> deserializer::readf32() {
    auto opt = read32();
    if (!opt) {
        return opt;
    }

    union {
        float f;
        uint32_t i;
    } mem{};
    mem.i = opt.value();
    return {mem.f};
}

std::optional<double> deserializer::readf64() {
    auto opt = read64();
    if (!opt) {
        return opt;
    }

    union {
        double d;
        uint32_t i;
    } mem{};
    mem.i = opt.value();
    return {mem.d};
}

std::optional<size_t> deserializer::read_size() {
    return read8();
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

std::optional<std::span<int32_t>> deserializer::read_arr_i32() {
    const auto size_opt = read_size();
    if (!size_opt) {
        return {};
    }

    const auto size = size_opt.value();
    expand_buffer(size * sizeof(int32_t));

    auto arr = reinterpret_cast<int32_t*>(m_data.get());
    for (int i = 0; i < size; ++i) {
        const auto value_opt = read32();
        if (!value_opt) {
            return {};
        }

        arr[i] = static_cast<int32_t>(value_opt.value());
    }

    return {{arr, size}};
}

std::optional<std::span<int64_t>> deserializer::read_arr_i64() {
    const auto size_opt = read_size();
    if (!size_opt) {
        return {};
    }

    const auto size = size_opt.value();
    expand_buffer(size * sizeof(int64_t));

    auto arr = reinterpret_cast<int64_t*>(m_data.get());
    for (int i = 0; i < size; ++i) {
        const auto value_opt = read64();
        if (!value_opt) {
            return {};
        }

        arr[i] = static_cast<int64_t >(value_opt.value());
    }

    return {{arr, size}};
}

std::optional<std::span<float>> deserializer::read_arr_f32() {
    const auto size_opt = read_size();
    if (!size_opt) {
        return {};
    }

    const auto size = size_opt.value();
    expand_buffer(size * sizeof(float));

    auto arr = reinterpret_cast<float*>(m_data.get());
    for (int i = 0; i < size; ++i) {
        const auto value_opt = readf32();
        if (!value_opt) {
            return {};
        }

        arr[i] = value_opt.value();
    }

    return {{arr, size}};
}

std::optional<std::span<double>> deserializer::read_arr_f64() {
    const auto size_opt = read_size();
    if (!size_opt) {
        return {};
    }

    const auto size = size_opt.value();
    expand_buffer(size * sizeof(double));

    auto arr = reinterpret_cast<double*>(m_data.get());
    for (int i = 0; i < size; ++i) {
        const auto value_opt = readf64();
        if (!value_opt) {
            return {};
        }

        arr[i] = value_opt.value();
    }

    return {{arr, size}};
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
        case value_type::string: {
            const auto value_opt = read_str();
            if (!value_opt) {
                return {};
            }

            return value::make_string(value_opt.value());
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
            const auto value_opt = read_arr_i32();
            if (!value_opt) {
                return {};
            }

            const auto value = value_opt.value();
            return value::make_int32_array(value);
        }
        case value_type::integer64_array: {
            const auto value_opt = read_arr_i64();
            if (!value_opt) {
                return {};
            }

            const auto value = value_opt.value();
            return value::make_int64_array(value);
        }
        case value_type::floating_point32_array: {
            const auto value_opt = read_arr_f32();
            if (!value_opt) {
                return {};
            }

            const auto value = value_opt.value();
            return value::make_float_array(value);
        }
        case value_type::floating_point64_array: {
            const auto value_opt = read_arr_f64();
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
    m_data_size = size;
}

serializer::serializer(writable_buffer* buffer)
    : m_buffer(buffer)
{}

bool serializer::write8(uint8_t value) {
    return write(m_buffer, value);
}

bool serializer::write16(uint16_t value) {
    value = obsr::bits::net16(value);
    return write(m_buffer, value);
}

bool serializer::write32(uint32_t value) {
    value = obsr::bits::net32(value);
    return write(m_buffer, value);
}

bool serializer::write64(uint64_t value) {
    value = obsr::bits::net64(value);
    return write(m_buffer, value);
}

bool serializer::writef32(float value) {
    union {
        float f;
        uint32_t i;
    } mem{};
    mem.f = value;

    return write32(mem.i);
}

bool serializer::writef64(double value) {
    union {
        double d;
        uint64_t i;
    } mem{};
    mem.d = value;

    return write64(mem.i);
}

bool serializer::write_size(size_t value) {
    if (!is_within_size_limits(value)) {
        return false;
    }

    return write8(value);
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

bool serializer::write_arr_i32(std::span<const int32_t> arr) {
    if (!write_size(arr.size())) {
        return false;
    }

    for (auto value : arr) {
        if (!write32(static_cast<uint32_t>(value))) {
            return false;
        }
    }

    return true;
}

bool serializer::write_arr_i64(std::span<const int64_t> arr) {
    if (!write_size(arr.size())) {
        return false;
    }

    for (auto value : arr) {
        if (!write64(static_cast<uint64_t>(value))) {
            return false;
        }
    }

    return true;
}

bool serializer::write_arr_f32(std::span<const float> arr) {
    if (!write_size(arr.size())) {
        return false;
    }

    for (auto value : arr) {
        if (!writef32(value)) {
            return false;
        }
    }

    return true;
}

bool serializer::write_arr_f64(std::span<const double> arr) {
    if (!write_size(arr.size())) {
        return false;
    }

    for (auto value : arr) {
        if (!writef64(value)) {
            return false;
        }
    }

    return true;
}

bool serializer::write_value(const value& value) {
    switch (value.get_type()) {
        case value_type::raw: {
            auto arr = value.get_raw();
            return write_raw(arr.data(), arr.size_bytes());
        }
        case value_type::string: {
            auto str = value.get_string();
            return write_str(str);
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
            return write_arr_i32(arr);
        }
        case value_type::integer64_array: {
            auto arr = value.get_int64_array();
            return write_arr_i64(arr);
        }
        case value_type::floating_point32_array: {
            auto arr = value.get_float_array();
            return write_arr_f32(arr);
        }
        case value_type::floating_point64_array: {
            auto arr = value.get_double_array();
            return write_arr_f64(arr);
        }
        case value_type::empty:
            return true;
        default:
            return false;
    }
}

}
