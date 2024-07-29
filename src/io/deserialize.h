#pragma once

#include <istream>
#include <string>
#include <span>

#include "obsr_types.h"

namespace obsr::io {

uint8_t read8(const std::istream& is);
uint16_t read16(const std::istream& is, uint16_t value);
uint32_t read32(const std::istream& is, uint32_t value);
float readf32(const std::istream& is, float value);
uint64_t read64(const std::istream& is, uint64_t value);
double readf64(const std::istream& is, double value);
std::string_view readstr(const std::istream& is);
void readraw(const std::istream& is, uint8_t* ptr, size_t& size);

value_type read(const std::istream& is);
void read(const std::istream& is, value_t& value);

}
