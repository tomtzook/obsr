
#include "serialize.h"


namespace obsr::io {

void write8(std::ostream& os, uint8_t value) {
    os << value;
}

void write16(std::ostream& os, uint16_t value) {
    os << value;
}

void write32(std::ostream& os, uint32_t value) {
    os << value;
}

void writef32(std::ostream& os, float value) {
    os << value;
}

void write64(std::ostream& os, uint64_t value) {
    os << value;
}

void writef64(std::ostream& os, double value) {
    os << value;
}

void writestr(std::ostream& os, const std::string_view& str) {
    // todo: improve as to not write the full 64bit always
    write64(os, str.size());
    os << str;
}

void writeraw(std::ostream& os, const uint8_t* ptr, size_t size) {
    // todo: improve as to not write the full 64bit always
    write64(os, size);
    os << ptr;
}

void write(std::ostream& os, value_type type) {
    // todo: specific serialized value (constant values)
}

void write(std::ostream& os, const value_t& value) {
    switch (value.type) {
        case value_type::empty:
            break;
        case value_type::raw:
            writeraw(os, value.value.raw.ptr, value.value.raw.size);
            break;
        case value_type::boolean:
            write8(os, value.value.boolean);
            break;
        case value_type::integer32:
            write32(os, value.value.integer);
            break;
        case value_type::integer64:
            write64(os, value.value.integer64);
            break;
        case value_type::floating_point32:
            writef32(os, value.value.floating_point);
            break;
        case value_type::floating_point64:
            writef64(os, value.value.floating_point64);
            break;
    }
}

}
