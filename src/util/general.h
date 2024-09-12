#pragma once

#include <memory>
#include <mutex>

#include "debug.h"

namespace obsr {

#define _LOG_MODULE_GENERAL "general"

template<typename... args_>
static void invoke_func_nolock(const std::function<void(args_...)>& ref, args_... args) {
    if (ref != nullptr) {
        try {
            ref(args...);
        } catch (const std::exception& e) {
            TRACE_ERROR(_LOG_MODULE_GENERAL, "Error while invoking func: what=%s", e.what());
        } catch (...) {
            TRACE_ERROR(_LOG_MODULE_GENERAL, "Error while invoking func: unknown");
        }
    }
}

template<typename obj_, typename... args_>
static void invoke_sharedptr_nolock(const std::shared_ptr<obj_>& ref, void(obj_::*func)(args_...), args_... args) {
    auto ptr = ref.get();
    if (ptr != nullptr) {
        try {
            (ptr->*func)(args...);
        } catch (const std::exception& e) {
            TRACE_ERROR(_LOG_MODULE_GENERAL, "Error while invoking func: what=%s", e.what());
        } catch (...) {
            TRACE_ERROR(_LOG_MODULE_GENERAL, "Error while invoking func: unknown");
        }
    }
}

}
