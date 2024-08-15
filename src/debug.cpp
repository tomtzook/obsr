
#include <cstdio>
#include <cstdarg>

#include "debug.h"


namespace obsr::debug {

void trace_impl(log_level level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}

}
