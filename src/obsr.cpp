
#include <cstring>

#include "obsr_internal.h"
#include "os/mutex.h"
#include "storage/storage.h"
#include "obsr.h"


namespace obsr {

struct _instance {
    os::shared_mutex mutex;
    storage::storage storage;
};

instance create() {
    auto _new_inst = new _instance;
    return _new_inst;
}

object get_root(instance instance) {
    os::mutex_guard guard(instance->mutex);

    return instance->storage.get_root();
}

object get_child(instance instance, object obj, std::string_view name) {
    os::mutex_guard guard(instance->mutex);

    return instance->storage.get_or_create_child(obj, name);
}

entry get_entry(instance instance, object obj, std::string_view name) {
    os::mutex_guard guard(instance->mutex);

    return instance->storage.get_or_create_entry(obj, name);
}

void get_value(instance instance, entry entry, value& value) {
    os::mutex_guard guard(instance->mutex);

    instance->storage.get_entry_value(entry, value);
}

void set_value(instance instance, entry entry, const value& value) {
    os::mutex_guard guard(instance->mutex);

    instance->storage.set_entry_value(entry, value);
}

}
