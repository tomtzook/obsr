#pragma once

#include <memory>
#include <mutex>

namespace obsr {

template<typename obj_, typename... args_>
static void invoke_ptr(std::unique_lock<std::mutex>& lock, obj_* ref, void(obj_::*func)(args_...), args_... args) {
    if (ref != nullptr) {
        auto ptr = ref;
        lock.unlock();
        try {
            (ptr->*func)(args...);
        } catch (...) {}
        lock.lock();
    }
}

}
