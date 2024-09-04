#pragma once

#include <memory>
#include <mutex>

namespace obsr {

template<typename obj_, typename... args_>
static void invoke_ptr(std::unique_lock<std::mutex>& lock, obj_* ref, void(obj_::*func)(args_...), args_... args) {
    auto ptr = ref;
    if (ptr != nullptr) {
        lock.unlock();
        try {
            (ptr->*func)(args...);
        } catch (...) {}
        lock.lock();
    }
}

template<typename obj_, typename... args_>
static void invoke_ptr_nolock(obj_* ref, void(obj_::*func)(args_...), args_... args) {
    auto ptr = ref;
    if (ptr != nullptr) {
        try {
            (ptr->*func)(args...);
        } catch (...) {}
    }
}

template<typename obj_, typename... args_>
static void invoke_shared_ptr(std::unique_lock<std::mutex>& lock, const std::shared_ptr<obj_>& ref, void(obj_::*func)(args_...), args_... args) {
    auto ptr = ref.get();
    if (ptr != nullptr) {
        lock.unlock();
        try {
            (ptr->*func)(args...);
        } catch (...) {}
        lock.lock();
    }
}

}
