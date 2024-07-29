#pragma once

#include <ostream>
#include <string>

#include "obsr_types.h"

namespace obsr::io {

void write8(std::ostream& os, uint8_t value);
void write16(std::ostream& os, uint16_t value);
void write32(std::ostream& os, uint32_t value);
void writef32(std::ostream& os, float value);
void write64(std::ostream& os, uint64_t value);
void writef64(std::ostream& os, double value);
void writestr(std::ostream& os, const std::string_view& str);
void writeraw(std::ostream& os, const uint8_t* ptr, size_t size);

void write(std::ostream& os, value_type type);
void write(std::ostream& os, const value_t& value);

}
