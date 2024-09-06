#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <chrono>
#include <cassert>
#include <span>
#include <memory>

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
    boolean,
    integer32,
    integer64,
    floating_point32,
    floating_point64,
    integer32_array
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

    inline void set_int32_array(std::span<const int32_t> value) {
        m_type = value_type::integer32_array;

        auto data = create_array(value);
        m_value.integer32_array.arr = data.get();
        m_value.integer32_array.size = value.size();
        m_data = std::move(data);
    }

    static inline value make() {
        return obsr::value{value_type::empty};
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

private:
    template<typename t_>
    static std::shared_ptr<t_[]> create_array(std::span<const t_> value) {
        auto data = std::shared_ptr<t_[]>(new t_[value.size()]);
        std::copy(value.begin(), value.end(), data.get());

        return data;
    }

    explicit value(value_type type)
        : m_type(type)
        , m_value()
    {}

    value_type m_type;
    union {
        bool boolean;
        int32_t integer32;
        int64_t integer64;
        float floating_point32;
        double floating_point64;
        struct {
            int32_t* arr;
            size_t size;
        } integer32_array;
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
    event(std::chrono::milliseconds timestamp, event_type type, std::string_view path, const value& old_value, const value& value)
        : m_timestamp(timestamp)
        , m_type(type)
        , m_path(path)
        , m_old_value(old_value)
        , m_value(value)
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
