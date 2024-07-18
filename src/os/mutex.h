#pragma once

#include <pthread.h>
#include <memory>

namespace obsr::os {

class mutex {
public:
    mutex();
    ~mutex();

    void lock();
    void unlock();

private:
    pthread_mutex_t m_mutex;
    pthread_mutexattr_t m_mutex_attr;
};

using shared_mutex = std::shared_ptr<mutex>;

class mutex_guard {
public:
    mutex_guard(const shared_mutex& mutex);
    ~mutex_guard();

private:
    shared_mutex m_mutex;
};

}
