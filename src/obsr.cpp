
#include <mutex>
#include <fmt/format.h>

#include "obsr_internal.h"
#include "storage/storage.h"
#include "obsr.h"


namespace obsr {

struct object_data {
    explicit object_data(const std::string_view& path)
        : path(path) {
    }

    std::string path;
};

struct _instance {
    _instance()
        : mutex()
        , listener_storage(std::make_shared<storage::listener_storage>())
        , storage(listener_storage)
        , objects()
        , object_paths()
        , root(objects.allocate_new("")) {
    }

    std::mutex mutex;
    storage::listener_storage_ref listener_storage;
    storage::storage storage;

    handle_table<object_data, 256> objects;
    std::map<std::string, object, std::less<>> object_paths;
    object root = empty_handle;
};

instance create() {
    auto _new_inst = new _instance;

    return _new_inst;
}

object get_root(instance instance) {
    std::unique_lock guard(instance->mutex);

    return instance->root;
}

object get_child(instance instance, object obj, std::string_view name) {
    std::unique_lock guard(instance->mutex);

    auto data = instance->objects[obj];
    const auto path = fmt::format("{}/{}", data->path, name);

    auto it = instance->object_paths.find(path);
    if (it == instance->object_paths.end()) {
        const auto handle = instance->objects.allocate_new(path);
        instance->object_paths.emplace(path, handle);

        return handle;
    } else {
        return it->second;
    }
}

entry get_entry(instance instance, object obj, std::string_view name) {
    std::unique_lock guard(instance->mutex);

    auto data = instance->objects[obj];
    const auto path = fmt::format("{}/{}", data->path, name);

    return instance->storage.get_or_create_entry(path);
}

void delete_object(instance instance, object obj) {
    std::unique_lock guard(instance->mutex);

    if (obj == instance->root) {
        throw cannot_delete_root_exception();
    }

    auto data = instance->objects[obj];
    instance->storage.delete_entries_in_path(data->path);

    instance->objects.release(obj);
}

void delete_entry(instance instance, entry entry) {
    std::unique_lock guard(instance->mutex);

    instance->storage.delete_entry(entry);
}

uint32_t probe(instance instance, entry entry) {
    std::unique_lock guard(instance->mutex);

    return instance->storage.probe(entry);
}

void get_value(instance instance, entry entry, value_t& value) {
    std::unique_lock guard(instance->mutex);

    instance->storage.get_entry_value(entry, value);
}

void set_value(instance instance, entry entry, const value_t& value) {
    std::unique_lock guard(instance->mutex);

    instance->storage.set_entry_value(entry, value);
}

listener listen_object(instance instance, object obj, const listener_callback&& callback) {
    std::unique_lock guard(instance->mutex);

    auto data = instance->objects[obj];
    return instance->storage.listen(data->path, callback);
}

listener listen_entry(instance instance, entry entry, const listener_callback&& callback) {
    std::unique_lock guard(instance->mutex);

    return instance->storage.listen(entry, callback);
}

void delete_listener(instance instance, listener listener) {
    std::unique_lock guard(instance->mutex);

    instance->storage.remove_listener(listener);
}

}
