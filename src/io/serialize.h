#pragma once

#include "obsr_types.h"
#include "buffer.h"

namespace obsr::io {

class deserializer {
public:
    template<readable t_>
    explicit deserializer(t_& t)
        : deserializer(create_read_func(t))
    {}

    [[nodiscard]] std::optional<uint8_t> read8();
    [[nodiscard]] std::optional<uint16_t> read16();
    [[nodiscard]] std::optional<uint32_t> read32();
    [[nodiscard]] std::optional<uint64_t> read64();
    [[nodiscard]] std::optional<float> readf32();
    [[nodiscard]] std::optional<double> readf64();
    [[nodiscard]] std::optional<size_t> read_size();
    [[nodiscard]] std::optional<std::span<uint8_t>> read_raw();
    [[nodiscard]] std::optional<std::string_view> read_str();
    [[nodiscard]] std::optional<std::span<int32_t>> read_arr_i32();
    [[nodiscard]] std::optional<std::span<int64_t>> read_arr_i64();
    [[nodiscard]] std::optional<std::span<float>> read_arr_f32();
    [[nodiscard]] std::optional<std::span<double>> read_arr_f64();
    [[nodiscard]] std::optional<obsr::value> read_value(value_type type);

private:
    explicit deserializer(read_func&& read);
    void expand_buffer(size_t size);

    read_func m_read;
    std::unique_ptr<uint8_t> m_data;
    size_t m_data_size;
};

class serializer {
public:
    template<writable t_>
    explicit serializer(t_& t)
        : serializer(create_write_func(t))
    {}

    [[nodiscard]] bool write8(uint8_t value);
    [[nodiscard]] bool write16(uint16_t value);
    [[nodiscard]] bool write32(uint32_t value);
    [[nodiscard]] bool write64(uint64_t value);
    [[nodiscard]] bool writef32(float value);
    [[nodiscard]] bool writef64(double value);
    [[nodiscard]] bool write_size(size_t value);
    [[nodiscard]] bool write_raw(const uint8_t* ptr, size_t size);
    [[nodiscard]] bool write_str(std::string_view str);
    [[nodiscard]] bool write_arr_i32(std::span<const int32_t> arr);
    [[nodiscard]] bool write_arr_i64(std::span<const int64_t> arr);
    [[nodiscard]] bool write_arr_f32(std::span<const float> arr);
    [[nodiscard]] bool write_arr_f64(std::span<const double> arr);
    [[nodiscard]] bool write_value(const value& value);

private:
    explicit serializer(write_func&& write);

    write_func m_write;
};

}
