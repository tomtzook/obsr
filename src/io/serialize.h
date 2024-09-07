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
    std::optional<std::span<uint8_t>> read_raw();
    std::optional<std::string_view> read_str();
    std::optional<obsr::value> read_value(value_type type);

    template<typename t_>
    inline std::optional<std::span<t_>> read_arr() {
        const auto value_opt = read_raw();
        if (!value_opt) {
            return {};
        }

        const auto value = value_opt.value();
        return {{reinterpret_cast<t_*>(value.data()), value.size_bytes() / sizeof(t_)}};
    }

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
    bool write_raw(const uint8_t* ptr, size_t size);
    bool write_str(std::string_view str);
    bool write_value(const value& value);

    template<typename t_>
    inline bool write_arr(std::span<const t_> arr) {
        return write_raw(reinterpret_cast<const uint8_t*>(arr.data()), arr.size_bytes());
    }

private:
    writable_buffer* m_buffer;
};

}
