
#include "obsr_types.h"
#include "obsr_except.h"

namespace obsr {

value::value(const value_type type)
    : m_type(type)
    , m_value()
    , m_data()
{}

value_type value::get_type() const {
    return m_type;
}

bool value::empty() const {
    return m_type == value_type::empty;
}

void value::clear() {
    m_type = value_type::empty;
    m_data.reset();
}

std::span<const uint8_t> value::get_raw() const {
    assert(m_type == value_type::raw);
    return {m_value.raw.ptr, m_value.raw.size};
}

std::span<const uint8_t> value::get_raw_or(const std::span<const uint8_t> default_val) const {
    if (m_type == value_type::raw) {
        return get_raw();
    }

    return default_val;
}

void value::set_raw(const std::span<const uint8_t> value) {
    m_type = value_type::raw;

    auto data = create_array(value);
    m_value.raw.ptr = data.get();
    m_value.raw.size = value.size();
    m_data = std::move(data);
}

std::string_view value::get_string() const {
    assert(m_type == value_type::string);
    return {m_value.string.ptr, m_value.string.size};
}

void value::set_string(const std::string_view value) {
    m_type = value_type::string;

    auto data = create_array<char>({
        reinterpret_cast<const char*>(value.data()), value.size()});
    m_value.string.ptr = data.get();
    m_value.string.size = value.size();
    m_data = std::move(data);
}

bool value::get_boolean() const {
    assert(m_type == value_type::boolean);
    return m_value.boolean;
}

bool value::get_boolean_or(const bool default_val) const {
    if (m_type == value_type::boolean) {
        return get_boolean();
    }

    return default_val;
}

void value::set_boolean(const bool value) {
    m_type = value_type::boolean;
    m_value.boolean = value;
}

int32_t value::get_int32() const {
    assert(m_type == value_type::integer32);
    return m_value.integer32;
}

int32_t value::get_int32_or(const int32_t default_val) const {
    if (m_type == value_type::integer32) {
        return get_int32();
    }

    return default_val;
}

void value::set_int32(const int32_t value) {
    m_type = value_type::integer32;
    m_value.integer32 = value;
}

int64_t value::get_int64() const {
    assert(m_type == value_type::integer64);
    return m_value.integer64;
}

int64_t value::get_int64_or(const int64_t default_val) const {
    if (m_type == value_type::integer64) {
        return get_int64();
    }

    return default_val;
}

void value::set_int64(const int64_t value) {
    m_type = value_type::integer64;
    m_value.integer64 = value;
}

float value::get_float() const {
    assert(m_type == value_type::floating_point32);
    return m_value.floating_point32;
}

float value::get_float_or(const float default_val) const {
    if (m_type == value_type::floating_point32) {
        return get_float();
    }

    return default_val;
}

void value::set_float(const float value) {
    m_type = value_type::floating_point32;
    m_value.floating_point32 = value;
}

double value::get_double() const {
    assert(m_type == value_type::floating_point64);
    return m_value.floating_point64;
}

double value::get_double_or(const double default_val) const {
    if (m_type == value_type::floating_point64) {
        return get_double();
    }

    return default_val;
}

void value::set_double(const double value) {
    m_type = value_type::floating_point64;
    m_value.floating_point64 = value;
}

std::span<const int32_t> value::get_int32_array() const {
    assert(m_type == value_type::integer32_array);
    return {m_value.integer32_array.arr, m_value.integer32_array.size};
}

std::span<const int32_t> value::get_int32_array_or(const std::span<const int32_t> default_val) const {
    if (m_type == value_type::integer32_array) {
        return get_int32_array();
    }

    return default_val;
}

void value::set_int32_array(const std::span<const int32_t> value) {
    m_type = value_type::integer32_array;

    auto data = create_array(value);
    m_value.integer32_array.arr = data.get();
    m_value.integer32_array.size = value.size();
    m_data = std::move(data);
}

std::span<const int64_t> value::get_int64_array() const {
    assert(m_type == value_type::integer64_array);
    return {m_value.integer64_array.arr, m_value.integer64_array.size};
}

std::span<const int64_t> value::get_int64_array_or(const std::span<const int64_t> default_val) const {
    if (m_type == value_type::integer64_array) {
        return get_int64_array();
    }

    return default_val;
}

void value::set_int64_array(const std::span<const int64_t> value) {
    m_type = value_type::integer64_array;

    auto data = create_array(value);
    m_value.integer64_array.arr = data.get();
    m_value.integer64_array.size = value.size();
    m_data = std::move(data);
}

std::span<const float> value::get_float_array() const {
    assert(m_type == value_type::floating_point32_array);
    return {m_value.floating_point32_array.arr, m_value.floating_point32_array.size};
}

std::span<const float> value::get_float_array_or(const std::span<const float> default_val) const {
    if (m_type == value_type::floating_point32_array) {
        return get_float_array();
    }

    return default_val;
}

void value::set_float_array(const std::span<const float> value) {
    m_type = value_type::floating_point32_array;

    auto data = create_array(value);
    m_value.floating_point32_array.arr = data.get();
    m_value.floating_point32_array.size = value.size();
    m_data = std::move(data);
}

std::span<const double> value::get_double_array() const {
    assert(m_type == value_type::floating_point64_array);
    return {m_value.floating_point64_array.arr, m_value.floating_point64_array.size};
}

std::span<const double> value::get_double_array_or(const std::span<const double> default_val) const {
    if (m_type == value_type::floating_point64_array) {
        return get_double_array();
    }

    return default_val;
}

void value::set_double_array(const std::span<const double> value) {
    m_type = value_type::floating_point64_array;

    auto data = create_array(value);
    m_value.floating_point64_array.arr = data.get();
    m_value.floating_point64_array.size = value.size();
    m_data = std::move(data);
}

value value::make() {
    return obsr::value{value_type::empty};
}

value value::make_raw(const std::span<const uint8_t> value) {
    obsr::value val(value_type::raw);
    val.set_raw(value);
    return std::move(val);
}

value value::make_raw(const void* ptr, size_t size) {
    return make_raw({reinterpret_cast<const uint8_t*>(ptr), size});
}

value value::make_string(const std::string_view value) {
    obsr::value val(value_type::string);
    val.set_string(value);
    return std::move(val);
}

value value::make_boolean(const bool value) {
    obsr::value val(value_type::boolean);
    val.set_boolean(value);
    return std::move(val);
}

value value::make_int32(const int32_t value) {
    obsr::value val(value_type::integer32);
    val.set_int32(value);
    return std::move(val);
}

value value::make_int64(const int64_t value) {
    obsr::value val(value_type::integer64);
    val.set_int64(value);
    return std::move(val);
}

value value::make_float(const float value) {
    obsr::value val(value_type::floating_point32);
    val.set_float(value);
    return std::move(val);
}

value value::make_double(const double value) {
    obsr::value val(value_type::floating_point64);
    val.set_double(value);
    return std::move(val);
}

value value::make_int32_array(const std::span<const int32_t> value) {
    obsr::value val(value_type::integer32_array);
    val.set_int32_array(value);
    return std::move(val);

}

value value::make_int32_array(const std::initializer_list<int32_t> value) {
    return make_int32_array(std::span(value.begin(), value.end()));
}

value value::make_int64_array(const std::span<const int64_t> value) {
    obsr::value val(value_type::integer64_array);
    val.set_int64_array(value);
    return std::move(val);
}

value value::make_int64_array(const std::initializer_list<int64_t> value) {
    return make_int64_array(std::span(value.begin(), value.end()));
}

value value::make_float_array(const std::span<const float> value) {
    obsr::value val(value_type::floating_point32_array);
    val.set_float_array(value);
    return std::move(val);
}

value value::make_float_array(const std::initializer_list<float> value) {
    return make_float_array(std::span(value.begin(), value.end()));
}

value value::make_double_array(const std::span<const double> value) {
    obsr::value val(value_type::floating_point64_array);
    val.set_double_array(value);
    return std::move(val);
}

value value::make_double_array(const std::initializer_list<double> value) {
    return make_double_array(std::span(value.begin(), value.end()));
}

void value::verify_within_size_limits(const size_t size) {
    if (size >= UINT8_MAX) {
        throw data_exceeds_size_limits_exception();
    }
}

event::event(std::chrono::milliseconds timestamp, event_type type, std::string_view path, obsr::entry entry)
    : m_timestamp(timestamp)
    , m_type(type)
    , m_path(path)
    , m_entry(entry)
    , m_old_value(value::make())
    , m_value(value::make())
{}

event::event(std::chrono::milliseconds timestamp, event_type type, std::string_view path, obsr::entry entry, value old_value, value value)
    : m_timestamp(timestamp)
    , m_type(type)
    , m_path(path)
    , m_entry(entry)
    , m_old_value(std::move(old_value))
    , m_value(std::move(value))
{}

std::chrono::milliseconds event::get_timestamp() const {
    return m_timestamp;
}

void event::set_timestamp(const std::chrono::milliseconds timestamp) {
    m_timestamp = timestamp;
}

event_type event::get_type() const {
    return m_type;
}

const std::string& event::get_path() const {
    return m_path;
}

obsr::entry event::get_entry() const {
    return m_entry;
}

const obsr::value& event::get_old_value() const {
    assert(m_type == event_type::value_changed);
    return m_old_value;
}

const obsr::value& event::get_value() const {
    assert(m_type == event_type::value_changed);
    return m_value;
}

}
