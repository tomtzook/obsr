#pragma once

#ifdef DEBUG
#define TRACE(level, module, format, ...) \
    do {                                  \
        if (can_log(level)) {             \
            obsr::debug::trace_impl(level, "(%s:%d) " module ": " format "\n", __FILE__, __LINE__, ##__VA_ARGS__);\
        }                                  \
    } while(0)

#define TRACE_DEBUG(module, format, ...) TRACE(obsr::debug::log_level_debug, module, format, ##__VA_ARGS__)
#define TRACE_INFO(module, format, ...) TRACE(obsr::debug::log_level_info, module, format, ##__VA_ARGS__)
#define TRACE_ERROR(module, format, ...) TRACE(obsr::debug::log_level_error, module, format, ##__VA_ARGS__)
#else
#define TRACE(level, module, format, ...)
#define TRACE_DEBUG(module, format, ...)
#define TRACE_INFO(module, format, ...)
#define TRACE_ERROR(module, format, ...)
#endif

#ifdef DEBUG
namespace obsr::debug {

enum log_level {
    log_level_debug,
    log_level_info,
    log_level_error
};

bool can_log(log_level level);
void trace_impl(log_level level, const char* format, ...);

}
#endif
