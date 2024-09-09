#pragma once

#include <endian.h>
#include <cstddef>
#include <cstdint>

namespace obsr::bits {

static inline uint16_t net16(uint16_t value) {
    return htobe16(value);
}

static inline uint32_t net32(uint32_t value) {
    return htobe32(value);
}

static inline uint64_t net64(uint64_t value) {
    return htobe64(value);
}

static inline uint16_t host16(uint16_t value) {
    return be16toh(value);
}

static inline uint32_t host32(uint32_t value) {
    return be32toh(value);
}

static inline uint64_t host64(uint64_t value) {
    return be64toh(value);
}

}
