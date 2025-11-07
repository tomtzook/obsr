#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <chrono>
#include <cassert>
#include <span>
#include <memory>
#include <utility>

namespace obsr {

using handle = uint32_t;
using object = handle;
using entry = handle;
using listener = handle;

constexpr handle empty_handle = static_cast<handle>(-1);
constexpr uint32_t entry_not_exists = static_cast<uint32_t>(-1);

enum class entry_flag : uint8_t {

};

enum class value_type : uint8_t {
    empty,
    raw,
    string,
    boolean,
    integer32,
    integer64,
    floating_point32,
    floating_point64,
    integer32_array,
    integer64_array,
    floating_point32_array,
    floating_point64_array
};

class value {
public:
    value(const value& other) = default;
    value(value&& other) = default;

    value& operator=(const value& other) = default;
    value& operator=(value&& other) = default;

    [[nodiscard]] inline value_type get_type() const;
    [[nodiscard]] inline bool empty() const;

    void clear();

    [[nodiscard]] inline std::span<const uint8_t> get_raw() const;
    [[nodiscard]] inline std::span<const uint8_t> get_raw_or(std::span<const uint8_t> default_val) const;
    inline void set_raw(std::span<const uint8_t> value);

    [[nodiscard]] inline std::string_view get_string() const;
    inline void set_string(std::string_view value);

    [[nodiscard]] inline bool get_boolean() const;
    [[nodiscard]] inline bool get_boolean_or(bool default_val) const;
    inline void set_boolean(bool value);

    [[nodiscard]] inline int32_t get_int32() const;
    [[nodiscard]] inline int32_t get_int32_or(int32_t default_val) const;
    inline void set_int32(int32_t value);

    [[nodiscard]] inline int64_t get_int64() const;
    [[nodiscard]] inline int64_t get_int64_or(int64_t default_val) const;
    inline void set_int64(int64_t value);

    [[nodiscard]] inline float get_float() const;
    [[nodiscard]] inline float get_float_or(float default_val) const;
    inline void set_float(float value);

    [[nodiscard]] inline double get_double() const;
    [[nodiscard]] inline double get_double_or(double default_val) const;
    inline void set_double(double value);

    [[nodiscard]] inline std::span<const int32_t> get_int32_array() const;
    [[nodiscard]] inline std::span<const int32_t> get_int32_array_or(std::span<const int32_t> default_val) const;
    inline void set_int32_array(std::span<const int32_t> value);

    [[nodiscard]] inline std::span<const int64_t> get_int64_array() const;
    [[nodiscard]] inline std::span<const int64_t> get_int64_array_or(std::span<const int64_t> default_val) const;
    inline void set_int64_array(std::span<const int64_t> value);

    [[nodiscard]] inline std::span<const float> get_float_array() const;
    [[nodiscard]] inline std::span<const float> get_float_array_or(std::span<const float> default_val) const;
    inline void set_float_array(std::span<const float> value);

    [[nodiscard]] inline std::span<const double> get_double_array() const;
    [[nodiscard]] inline std::span<const double> get_double_array_or(std::span<const double> default_val) const;
    inline void set_double_array(std::span<const double> value);

    static inline value make();
    static inline value make_raw(std::span<const uint8_t> value);
    static inline value make_raw(const void* ptr, size_t size);
    static inline value make_string(std::string_view value);
    static inline value make_boolean(bool value);
    static inline value make_int32(int32_t value);
    static inline value make_int64(int64_t value);
    static inline value make_float(float value);
    static inline value make_double(double value);
    static inline value make_int32_array(std::span<const int32_t> value);
    static inline value make_int32_array(std::initializer_list<int32_t> value);
    static inline value make_int64_array(std::span<const int64_t> value);
    static inline value make_int64_array(std::initializer_list<int64_t> value);
    static inline value make_float_array(std::span<const float> value);
    static inline value make_float_array(std::initializer_list<float> value);
    static inline value make_double_array(std::span<const double> value);
    static inline value make_double_array(std::initializer_list<double> value);

private:
    template<typename t_>
    static std::shared_ptr<t_[]> create_array(std::span<const t_> value) {
        verify_within_size_limits(value.size());

        auto data = std::shared_ptr<t_[]>(new t_[value.size()]);
        std::copy(value.begin(), value.end(), data.get());

        return data;
    }

    static void verify_within_size_limits(size_t size);

    explicit value(value_type type);

    value_type m_type;
    union {
        struct {
            uint8_t* ptr;
            size_t size;
        } raw;
        struct {
            char* ptr;
            size_t size;
        } string;
        bool boolean;
        int32_t integer32;
        int64_t integer64;
        float floating_point32;
        double floating_point64;
        struct {
            int32_t* arr;
            size_t size;
        } integer32_array;
        struct {
            int64_t* arr;
            size_t size;
        } integer64_array;
        struct {
            float* arr;
            size_t size;
        } floating_point32_array;
        struct {
            double* arr;
            size_t size;
        } floating_point64_array;
    } m_value;
    std::shared_ptr<void> m_data;
};

enum class event_type {
    created = 1,
    deleted,
    value_changed
};

class event {
public:
    event(std::chrono::milliseconds timestamp, event_type type, std::string_view path, obsr::entry entry);
    event(std::chrono::milliseconds timestamp, event_type type, std::string_view path, obsr::entry entry, value old_value, value value);
    event(const event& other) = default;
    event(event&& other) = default;

    event& operator=(const event& other) = default;
    event& operator=(event&& other) = default;

    [[nodiscard]] std::chrono::milliseconds get_timestamp() const;
    void set_timestamp(std::chrono::milliseconds timestamp);

    [[nodiscard]] inline event_type get_type() const;
    [[nodiscard]] inline const std::string& get_path() const;
    [[nodiscard]] inline obsr::entry get_entry() const;
    [[nodiscard]] inline const obsr::value& get_old_value() const;
    [[nodiscard]] inline const obsr::value& get_value() const;

private:
    std::chrono::milliseconds m_timestamp;
    event_type m_type;
    std::string m_path;
    obsr::entry m_entry;

    // available for value change events
    obsr::value m_old_value;
    obsr::value m_value;
};

using listener_callback = std::function<void(const event&)>;

}
