
#include <cerrno>

#include "obsr_except.h"
#include "mutex.h"


namespace obsr::os {

mutex::mutex()
    : m_mutex()
    , m_mutex_attr() {
    if (pthread_mutexattr_init(&m_mutex_attr)) {
        throw os_exception();
    }

    if (pthread_mutexattr_settype(&m_mutex_attr, PTHREAD_MUTEX_RECURSIVE)) {
        const auto code = errno;
        pthread_mutexattr_destroy(&m_mutex_attr);
        throw os_exception(code);
    }

    if (pthread_mutex_init(&m_mutex, &m_mutex_attr)) {
        const auto code = errno;
        pthread_mutexattr_destroy(&m_mutex_attr);
        throw os_exception(code);
    }
}

mutex::~mutex() {
    pthread_mutex_destroy(&m_mutex);
    pthread_mutexattr_destroy(&m_mutex_attr);
}

void mutex::lock() {
    pthread_mutex_lock(&m_mutex);
}

void mutex::unlock() {
    pthread_mutex_unlock(&m_mutex);
}

mutex_guard::mutex_guard(const shared_mutex& mutex)
    : m_mutex(mutex) {
    m_mutex->lock();
}

mutex_guard::~mutex_guard() {
    m_mutex->unlock();
}

}
