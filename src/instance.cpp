
#include "instance.h"

namespace obsr {

object_data::object_data(const std::string_view& path)
    : path(path) {
}

instance::instance()
    : m_mutex()
    , m_listener_storage(std::make_shared<storage::listener_storage>())
    , m_storage(m_listener_storage)
    , m_objects()
    , m_object_paths()
    , m_root(m_objects.allocate_new("")) {
}

object instance::get_root() {
    std::unique_lock guard(m_mutex);

    return m_root;
}

object instance::get_child(object obj, std::string_view name) {
    std::unique_lock guard(m_mutex);

    auto data = m_objects[obj];
    const auto path = fmt::format("{}/{}", data->path, name);

    auto it = m_object_paths.find(path);
    if (it == m_object_paths.end()) {
        const auto handle = m_objects.allocate_new(path);
        m_object_paths.emplace(path, handle);

        return handle;
    } else {
        return it->second;
    }
}

entry instance::get_entry(object obj, std::string_view name) {
    std::unique_lock guard(m_mutex);

    auto data = m_objects[obj];
    const auto path = fmt::format("{}/{}", data->path, name);

    return m_storage.get_or_create_entry(path);
}

void instance::delete_object(object obj) {
    std::unique_lock guard(m_mutex);

    if (obj == m_root) {
        throw cannot_delete_root_exception();
    }

    auto data = m_objects[obj];
    m_storage.delete_entries(data->path);

    m_objects.release(obj);
}

void instance::delete_entry(entry entry) {
    std::unique_lock guard(m_mutex);

    m_storage.delete_entry(entry);
}

uint32_t instance::probe(entry entry) {
    std::unique_lock guard(m_mutex);

    return m_storage.probe(entry);
}

void instance::get_value(entry entry, value_t& value) {
    std::unique_lock guard(m_mutex);

    m_storage.get_entry_value(entry, value);
}

void instance::set_value(entry entry, const value_t& value) {
    std::unique_lock guard(m_mutex);

    m_storage.set_entry_value(entry, value);
}

void instance::clear_value(entry entry) {
    std::unique_lock guard(m_mutex);

    m_storage.clear_entry(entry);
}

listener instance::listen_object(object obj, const listener_callback& callback) {
    std::unique_lock guard(m_mutex);

    auto data = m_objects[obj];
    return m_storage.listen(data->path, callback);
}

listener instance::listen_entry(entry entry, const listener_callback& callback) {
    std::unique_lock guard(m_mutex);

    return m_storage.listen(entry, callback);
}

void instance::delete_listener(listener listener) {
    std::unique_lock guard(m_mutex);

    m_storage.remove_listener(listener);
}

}
