#pragma once

#include <ostream>
#include <string>

#include "obsr_types.h"
#include "buffer.h"

namespace obsr::io {

class deserializer {
public:
    explicit deserializer(readable_buffer* buffer);

    std::optional<uint8_t> read8();
    std::optional<uint16_t> read16();
    std::optional<uint32_t> read32();
    std::optional<uint64_t> read64();
    std::optional<float> readf32();
    std::optional<double> readf64();
    std::optional<size_t> read_size();
    std::optional<std::span<uint8_t>> read_raw();
    std::optional<std::string_view> read_str();
    std::optional<std::span<int32_t>> read_arr_i32();
    std::optional<std::span<int64_t>> read_arr_i64();
    std::optional<std::span<float>> read_arr_f32();
    std::optional<std::span<double>> read_arr_f64();
    std::optional<obsr::value> read_value(value_type type);

private:
    void expand_buffer(size_t size);

    readable_buffer* m_buffer;
    std::unique_ptr<uint8_t> m_data;
    size_t m_data_size;
};

class serializer {
public:
    explicit serializer(writable_buffer* buffer);

    bool write8(uint8_t value);
    bool write16(uint16_t value);
    bool write32(uint32_t value);
    bool write64(uint64_t value);
    bool writef32(float value);
    bool writef64(double value);
    bool write_size(size_t value);
    bool write_raw(const uint8_t* ptr, size_t size);
    bool write_str(std::string_view str);
    bool write_arr_i32(std::span<const int32_t> arr);
    bool write_arr_i64(std::span<const int64_t> arr);
    bool write_arr_f32(std::span<const float> arr);
    bool write_arr_f64(std::span<const double> arr);
    bool write_value(const value& value);

private:
    writable_buffer* m_buffer;
};

}
