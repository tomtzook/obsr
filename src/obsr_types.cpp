
#include "obsr_types.h"
#include "obsr_except.h"

namespace obsr {

void value::verify_within_size_limits(size_t size) {
    if (size >= UINT8_MAX) {
        throw data_exceeds_size_limits_exception();
    }
}

}
