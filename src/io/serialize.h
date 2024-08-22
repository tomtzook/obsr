#pragma once

#include <ostream>
#include <string>

#include "obsr_types.h"
#include "buffer.h"

namespace obsr::io {

bool read8(readable_buffer& buf, uint8_t& value);
bool read16(readable_buffer& buf, uint16_t& value);
bool read32(readable_buffer& buf, uint32_t& value);
bool readf32(readable_buffer& buf, float& value);
bool read64(readable_buffer& buf, uint64_t& value);
bool readf64(readable_buffer& buf, double& value);
bool readraw(readable_buffer& buf, uint8_t* ptr, size_t& size);
bool read(readable_buffer& buf, value_type type, value_raw& value);

bool write8(writable_buffer& buf, uint8_t value);
bool write16(writable_buffer& buf, uint16_t value);
bool write32(writable_buffer& buf, uint32_t value);
bool writef32(writable_buffer& buf, float value);
bool write64(writable_buffer& buf, uint64_t value);
bool writef64(writable_buffer& buf, double value);
bool writeraw(writable_buffer& buf, const uint8_t* ptr, size_t size);
bool write(writable_buffer& buf, value_type type, const value_raw& value);

}
