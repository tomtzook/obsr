
#include "obsr_except.h"
#include "storage.h"


namespace obsr::storage {

storage_entry::storage_entry(entry handle, const std::string_view& path)
    : m_handle(handle)
    , m_path(path)
    , m_value() {
}

bool storage_entry::is_in(const std::string_view& path) const {
    return m_path.find(path) >= 0;
}

std::string_view storage_entry::get_path() const {
    return m_path;
}

void storage_entry::get_value(value_t& value) const {
    value = m_value;
}

value_t storage_entry::set_value(const value_t& value) {
    const auto old_type = m_value.type;
    const auto new_type = value.type;
    if (old_type != value_type::empty && old_type != new_type) {
        throw entry_type_mismatch_exception(m_handle, old_type, new_type);
    }

    auto old = m_value;
    m_value = value;
    return old;
}

void storage_entry::clear() {
    m_value = value_t{};
}

storage::storage(listener_storage_ref& listener_storage)
    : m_listener_storage(listener_storage)
    , m_mutex()
    , m_entries()
    , m_paths() {
}

entry storage::get_or_create_entry(const std::string_view& path) {
    std::unique_lock guard(m_mutex);

    auto it = m_paths.find(path);
    if (it == m_paths.end()) {
        return create_new_entry(path);
    }

    const auto entry_handle = it->second;
    if (m_entries.has(entry_handle)) {
        return entry_handle;
    }

    // todo: better handling
    throw no_such_handle_exception(entry_handle);
}

void storage::delete_entry(entry entry) {
    std::unique_lock guard(m_mutex);

    auto data = m_entries.release(entry);
    auto path = data->get_path();

    m_listener_storage->notify(
            event_type::deleted,
            path);
}

void storage::delete_entries(const std::string_view& path) {
    std::unique_lock guard(m_mutex);

    std::vector<entry> handles;
    for (auto [handle, data] : m_entries) {
        if (data.is_in(path)) {
            handles.push_back(handle);
        }
    }

    for (auto handle : handles) {
        m_entries.release(handle);
    }

    m_listener_storage->notify(
            event_type::deleted,
            path);
}

uint32_t storage::probe(entry entry) {
    std::unique_lock guard(m_mutex);

    if (!m_entries.has(entry)) {
        return entry_not_exists;
    }

    // TODO: GET ENTRY FLAGS
    return 0;
}

void storage::get_entry_value(entry entry, value_t& value) {
    std::unique_lock guard(m_mutex);

    auto data = m_entries[entry];
    data->get_value(value);
}

void storage::set_entry_value(entry entry, const value_t& value) {
    std::unique_lock guard(m_mutex);

    auto data = m_entries[entry];
    auto old_value = data->set_value(value);

    m_listener_storage->notify(
            event_type::value_change,
            data->get_path(),
            old_value,
            value);
}

void storage::clear_entry(entry entry) {
    std::unique_lock guard(m_mutex);

    auto data = m_entries[entry];
    data->clear();

    m_listener_storage->notify(
            event_type::cleared,
            data->get_path());
}

listener storage::listen(entry entry, const listener_callback& callback) {
    std::unique_lock guard(m_mutex);

    auto data = m_entries[entry];
    return m_listener_storage->create_listener(callback, data->get_path());
}

listener storage::listen(const std::string_view& prefix, const listener_callback& callback) {
    std::unique_lock guard(m_mutex);

    return m_listener_storage->create_listener(callback, prefix);
}

void storage::remove_listener(listener listener) {
    std::unique_lock guard(m_mutex);

    m_listener_storage->destroy_listener(listener);
}

entry storage::create_new_entry(const std::string_view& path) {
    auto entry = m_entries.allocate_new_with_handle(path);
    auto data = m_entries[entry];

    m_paths.emplace(path, entry);

    m_listener_storage->notify(
            event_type::created,
            data->get_path());

    return entry;
}

}
