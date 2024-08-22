#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <chrono>
#include <cassert>

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
    floating_point64
};

struct value_raw {
    value_type type;
    union {
        bool boolean;
        int32_t integer;
        int64_t integer64;
        float floating_point;
        double floating_point64;
    } value;
};

class value {
public:
    explicit value(const value_raw& raw)
        : m_value(raw)
    {}
    value(const value& other) = default;
    value(value&& other) = default;

    value& operator=(const value& other) = default;
    value& operator=(value&& other) = default;

    [[nodiscard]] inline const value_raw& get_raw() const {
        return m_value;
    }

    [[nodiscard]] inline value_raw& get_raw() {
        return m_value;
    }

    [[nodiscard]] inline value_type get_type() const {
        return m_value.type;
    }

    [[nodiscard]] inline bool empty() const {
        return m_value.type == value_type::empty;
    }

    void clear() {
        m_value.type = value_type::empty;
    }

    [[nodiscard]] inline bool get_boolean() const {
        assert(m_value.type == value_type::boolean);
        return m_value.value.boolean;
    }

    inline void set_boolean(bool value) {
        m_value.type = value_type::boolean;
        m_value.value.boolean = value;
    }

    [[nodiscard]] inline int32_t get_int32() const {
        assert(m_value.type == value_type::integer32);
        return m_value.value.integer;
    }

    inline void set_int32(int32_t value) {
        m_value.type = value_type::integer32;
        m_value.value.integer = value;
    }

    [[nodiscard]] inline int64_t get_int64() const {
        assert(m_value.type == value_type::integer64);
        return m_value.value.integer64;
    }

    inline void set_int64(int64_t value) {
        m_value.type = value_type::integer64;
        m_value.value.integer64 = value;
    }

    [[nodiscard]] inline float get_float() const {
        assert(m_value.type == value_type::floating_point32);
        return m_value.value.floating_point;
    }

    inline void set_float(float value) {
        m_value.type = value_type::floating_point32;
        m_value.value.floating_point = value;
    }

    [[nodiscard]] inline double get_double() const {
        assert(m_value.type == value_type::floating_point64);
        return m_value.value.floating_point64;
    }

    inline void set_double(double value) {
        m_value.type = value_type::floating_point64;
        m_value.value.floating_point64 = value;
    }

    static inline value make() {
        obsr::value_raw val{.type = value_type::empty};
        return obsr::value(val);
    }

    static inline value make_boolean(bool value) {
        obsr::value_raw val{.type = value_type::boolean};
        val.value.boolean = value;
        return obsr::value(val);
    }

    static inline value make_int32(int32_t value) {
        obsr::value_raw val{.type = value_type::integer32};
        val.value.integer = value;
        return obsr::value(val);
    }

    static inline value make_int64(int64_t value) {
        obsr::value_raw val{.type = value_type::integer64};
        val.value.integer64 = value;
        return obsr::value(val);
    }

    static inline value make_float(float value) {
        obsr::value_raw val{.type = value_type::floating_point32};
        val.value.floating_point = value;
        return obsr::value(val);
    }

    static inline value make_double(double value) {
        obsr::value_raw val{.type = value_type::floating_point64};
        val.value.floating_point64 = value;
        return obsr::value(val);
    }
private:
    value()
        : m_value({.type = value_type::empty})
    {}

    value_raw m_value;
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
