#pragma once

#define TRACE(level, module, format, ...) obsr::debug::trace_impl(level, "(%s:%d) " module ": " format, __FILE__, __LINE__, ##__VA_ARGS__)
#define TRACE_DEBUG(module, format, ...) TRACE(obsr::debug::log_level_debug, module, format, ##__VA_ARGS__)
#define TRACE_INFO(module, format, ...) TRACE(obsr::debug::log_level_info, module, format, ##__VA_ARGS__)
#define TRACE_ERROR(module, format, ...) TRACE(obsr::debug::log_level_error, module, format, ##__VA_ARGS__)


namespace obsr::debug {

enum log_level {
    log_level_debug,
    log_level_info,
    log_level_error
};

void trace_impl(log_level level, const char* format, ...);

}
