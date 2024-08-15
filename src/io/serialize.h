#pragma once

#include <ostream>
#include <string>

#include "obsr_types.h"
#include "buffer.h"

namespace obsr::io {

bool read8(buffer& buf, uint8_t& value);
bool read16(buffer& buf, uint16_t& value);
bool read32(buffer& buf, uint32_t& value);
bool readf32(buffer& buf, float& value);
bool read64(buffer& buf, uint64_t& value);
bool readf64(buffer& buf, double& value);
bool readraw(buffer& buf, uint8_t* ptr, size_t& size);
bool read(buffer& buf, value_type type, value_t& value);

bool write8(buffer& buf, uint8_t value);
bool write16(buffer& buf, uint16_t value);
bool write32(buffer& buf, uint32_t value);
bool writef32(buffer& buf, float value);
bool write64(buffer& buf, uint64_t value);
bool writef64(buffer& buf, double value);
bool writestr(buffer& buf, const std::string_view& str);
bool writeraw(buffer& buf, const uint8_t* ptr, size_t size);
bool write(buffer& buf, const value_t& value);

}
