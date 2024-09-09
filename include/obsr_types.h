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

    [[nodiscard]] inline value_type get_type() const {
        return m_type;
    }

    [[nodiscard]] inline bool empty() const {
        return m_type == value_type::empty;
    }

    void clear() {
        m_type = value_type::empty;
        m_data.reset();
    }

    [[nodiscard]] inline std::span<const uint8_t> get_raw() const {
        assert(m_type == value_type::raw);
        return {m_value.raw.ptr, m_value.raw.size};
    }

    [[nodiscard]] inline std::span<const uint8_t> get_raw_or(std::span<const uint8_t> default_val) const {
        if (m_type == value_type::raw) {
            return get_raw();
        }
        return default_val;
    }

    inline void set_raw(std::span<const uint8_t> value) {
        m_type = value_type::raw;

        auto data = create_array(value);
        m_value.raw.ptr = data.get();
        m_value.raw.size = value.size();
        m_data = std::move(data);
    }

    [[nodiscard]] inline bool get_boolean() const {
        assert(m_type == value_type::boolean);
        return m_value.boolean;
    }

    [[nodiscard]] inline bool get_boolean_or(bool default_val) const {
        if (m_type == value_type::boolean) {
            return get_boolean();
        }
        return default_val;
    }

    inline void set_boolean(bool value) {
        m_type = value_type::boolean;
        m_value.boolean = value;
    }

    [[nodiscard]] inline int32_t get_int32() const {
        assert(m_type == value_type::integer32);
        return m_value.integer32;
    }

    [[nodiscard]] inline int32_t get_int32_or(int32_t default_val) const {
        if (m_type == value_type::integer32) {
            return get_int32();
        }
        return default_val;
    }

    inline void set_int32(int32_t value) {
        m_type = value_type::integer32;
        m_value.integer32 = value;
    }

    [[nodiscard]] inline int64_t get_int64() const {
        assert(m_type == value_type::integer64);
        return m_value.integer64;
    }

    [[nodiscard]] inline int64_t get_int64_or(int64_t default_val) const {
        if (m_type == value_type::integer64) {
            return get_int64();
        }
        return default_val;
    }

    inline void set_int64(int64_t value) {
        m_type = value_type::integer64;
        m_value.integer64 = value;
    }

    [[nodiscard]] inline float get_float() const {
        assert(m_type == value_type::floating_point32);
        return m_value.floating_point32;
    }

    [[nodiscard]] inline float get_float_or(float default_val) const {
        if (m_type == value_type::floating_point32) {
            return get_float();
        }
        return default_val;
    }

    inline void set_float(float value) {
        m_type = value_type::floating_point32;
        m_value.floating_point32 = value;
    }

    [[nodiscard]] inline double get_double() const {
        assert(m_type == value_type::floating_point64);
        return m_value.floating_point64;
    }

    [[nodiscard]] inline double get_double_or(double default_val) const {
        if (m_type == value_type::floating_point64) {
            return get_double();
        }
        return default_val;
    }

    inline void set_double(double value) {
        m_type = value_type::floating_point64;
        m_value.floating_point64 = value;
    }

    [[nodiscard]] inline std::span<const int32_t> get_int32_array() const {
        assert(m_type == value_type::integer32_array);
        return {m_value.integer32_array.arr, m_value.integer32_array.size};
    }

    [[nodiscard]] inline std::span<const int32_t> get_int32_array_or(std::span<const int32_t> default_val) const {
        if (m_type == value_type::integer32_array) {
            return get_int32_array();
        }
        return default_val;
    }

    inline void set_int32_array(std::span<const int32_t> value) {
        m_type = value_type::integer32_array;

        auto data = create_array(value);
        m_value.integer32_array.arr = data.get();
        m_value.integer32_array.size = value.size();
        m_data = std::move(data);
    }

    [[nodiscard]] inline std::span<const int64_t> get_int64_array() const {
        assert(m_type == value_type::integer64_array);
        return {m_value.integer64_array.arr, m_value.integer64_array.size};
    }

    [[nodiscard]] inline std::span<const int64_t> get_int64_array_or(std::span<const int64_t> default_val) const {
        if (m_type == value_type::integer64_array) {
            return get_int64_array();
        }
        return default_val;
    }

    inline void set_int64_array(std::span<const int64_t> value) {
        m_type = value_type::integer64_array;

        auto data = create_array(value);
        m_value.integer64_array.arr = data.get();
        m_value.integer64_array.size = value.size();
        m_data = std::move(data);
    }

    [[nodiscard]] inline std::span<const float> get_float_array() const {
        assert(m_type == value_type::floating_point32_array);
        return {m_value.floating_point32_array.arr, m_value.floating_point32_array.size};
    }

    [[nodiscard]] inline std::span<const float> get_float_array_or(std::span<const float> default_val) const {
        if (m_type == value_type::floating_point32_array) {
            return get_float_array();
        }
        return default_val;
    }

    inline void set_float_array(std::span<const float> value) {
        m_type = value_type::integer64_array;

        auto data = create_array(value);
        m_value.floating_point32_array.arr = data.get();
        m_value.floating_point32_array.size = value.size();
        m_data = std::move(data);
    }

    [[nodiscard]] inline std::span<const double> get_double_array() const {
        assert(m_type == value_type::floating_point64_array);
        return {m_value.floating_point64_array.arr, m_value.floating_point64_array.size};
    }

    [[nodiscard]] inline std::span<const double> get_double_array_or(std::span<const double> default_val) const {
        if (m_type == value_type::floating_point64_array) {
            return get_double_array();
        }
        return default_val;
    }

    inline void set_double_array(std::span<const double> value) {
        m_type = value_type::integer64_array;

        auto data = create_array(value);
        m_value.floating_point64_array.arr = data.get();
        m_value.floating_point64_array.size = value.size();
        m_data = std::move(data);
    }

    static inline value make() {
        return obsr::value{value_type::empty};
    }

    static inline value make_raw(std::span<const uint8_t> value) {
        obsr::value val(value_type::raw);
        val.set_raw(value);
        return std::move(val);
    }

    static inline value make_raw(const void* ptr, size_t size) {
        return make_raw({reinterpret_cast<const uint8_t*>(ptr), size});
    }

    static inline value make_boolean(bool value) {
        obsr::value val(value_type::boolean);
        val.set_boolean(value);
        return std::move(val);
    }

    static inline value make_int32(int32_t value) {
        obsr::value val(value_type::integer32);
        val.set_int32(value);
        return std::move(val);
    }

    static inline value make_int64(int64_t value) {
        obsr::value val(value_type::floating_point64);
        val.set_int64(value);
        return std::move(val);
    }

    static inline value make_float(float value) {
        obsr::value val(value_type::floating_point32);
        val.set_float(value);
        return std::move(val);
    }

    static inline value make_double(double value) {
        obsr::value val(value_type::floating_point64);
        val.set_double(value);
        return std::move(val);
    }

    static inline value make_int32_array(std::span<const int32_t> value) {
        obsr::value val(value_type::integer32_array);
        val.set_int32_array(value);
        return std::move(val);
    }

    static inline value make_int32_array(std::initializer_list<int32_t> value) {
        return make_int32_array(std::span(value.begin(), value.end()));
    }

    static inline value make_int64_array(std::span<const int64_t> value) {
        obsr::value val(value_type::integer64_array);
        val.set_int64_array(value);
        return std::move(val);
    }

    static inline value make_int64_array(std::initializer_list<int64_t> value) {
        return make_int64_array(std::span(value.begin(), value.end()));
    }

    static inline value make_float_array(std::span<const float> value) {
        obsr::value val(value_type::floating_point32_array);
        val.set_float_array(value);
        return std::move(val);
    }

    static inline value make_float_array(std::initializer_list<float> value) {
        return make_float_array(std::span(value.begin(), value.end()));
    }

    static inline value make_double_array(std::span<const double> value) {
        obsr::value val(value_type::floating_point64_array);
        val.set_double_array(value);
        return std::move(val);
    }

    static inline value make_double_array(std::initializer_list<double> value) {
        return make_double_array(std::span(value.begin(), value.end()));
    }

private:
    template<typename t_>
    static std::shared_ptr<t_[]> create_array(std::span<const t_> value) {
        verify_within_size_limits(value.size());

        auto data = std::shared_ptr<t_[]>(new t_[value.size()]);
        std::copy(value.begin(), value.end(), data.get());

        return data;
    }

    static void verify_within_size_limits(size_t size);

    explicit value(value_type type)
        : m_type(type)
        , m_value()
        , m_data()
    {}

    value_type m_type;
    union {
        struct {
            uint8_t* ptr;
            size_t size;
        } raw;
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
    event(std::chrono::milliseconds timestamp, event_type type, std::string_view path)
        : m_timestamp(timestamp)
        , m_type(type)
        , m_path(path)
        , m_old_value(value::make())
        , m_value(value::make())
    {}
    event(std::chrono::milliseconds timestamp, event_type type, std::string_view path, value old_value, value value)
        : m_timestamp(timestamp)
        , m_type(type)
        , m_path(path)
        , m_old_value(std::move(old_value))
        , m_value(std::move(value))
    {}
    event(const event& other) = default;
    event(event&& other) = default;

    event& operator=(const event& other) = default;
    event& operator=(event&& other) = default;

    [[nodiscard]] std::chrono::milliseconds get_timestamp() const {
        return m_timestamp;
    }

    [[nodiscard]] inline event_type get_type() const {
        return m_type;
    }

    [[nodiscard]] inline const std::string& get_path() const {
        return m_path;
    }

    [[nodiscard]] inline const obsr::value& get_old_value() const {
        assert(m_type == event_type::value_changed);
        return m_old_value;
    }

    [[nodiscard]] inline const obsr::value& get_value() const {
        assert(m_type == event_type::value_changed);
        return m_value;
    }

private:
    std::chrono::milliseconds m_timestamp;
    event_type m_type;
    std::string m_path;

    // available for value_raw change events
    obsr::value m_old_value;
    obsr::value m_value;
};

using listener_callback = std::function<void(const event&)>;

}
