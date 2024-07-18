#pragma once

#include <cstdint>
#include <cstddef>

namespace obsr {

struct _instance;

using instance = _instance*;
using handle = uint32_t;
using object = handle;
using entry = handle;

constexpr handle empty_handle = static_cast<handle>(-1);

enum class value_type {
    empty,
    raw,
    boolean,
    integer32,
    integer64,
    floating_point32,
    floating_point64
};

struct value {
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

}
