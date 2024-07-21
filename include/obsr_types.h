#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <chrono>

namespace obsr {

using handle = uint32_t;
using object = handle;
using entry = handle;
using listener = handle;

constexpr handle empty_handle = static_cast<handle>(-1);
constexpr uint32_t entry_not_exists = static_cast<uint32_t>(-1);

enum class entry_flag {

};

enum class value_type {
    empty,
    raw,
    boolean,
    integer32,
    integer64,
    floating_point32,
    floating_point64
};

struct value_t {
    value_type type;

    union {
        struct {
            uint8_t* ptr;
            size_t size;
        } raw;

        bool boolean;
        int32_t integer;
        int64_t integer64;
        float floating_point;
        double floating_point64;
    } value;
};

enum class event_type {
    created,
    deleted,
    value_change,
    cleared
};

struct event {
    std::chrono::milliseconds timestamp;
    event_type type;
    std::string path;

    // available for value change events
    value_t old_value;
    value_t value;
};

using listener_callback = std::function<void(const event&)>;

}
