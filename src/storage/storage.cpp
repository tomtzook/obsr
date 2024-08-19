
#include "obsr_except.h"
#include "storage.h"


namespace obsr::storage {

storage_entry::storage_entry(entry handle, const std::string_view& path)
    : m_handle(handle)
    , m_path(path)
    , m_value()
    , m_net_id(id_not_assigned)
    , m_flags(0) {
}

bool storage_entry::is_in(const std::string_view& path) const {
    return m_path.find(path) >= 0;
}

std::string_view storage_entry::get_path() const {
    return m_path;
}

entry_id storage_entry::get_net_id() const {
    return m_net_id;
}

void storage_entry::set_net_id(entry_id id) {
    m_net_id = id;
}

void storage_entry::clear_net_id() {
    m_net_id = id_not_assigned;
}

uint16_t storage_entry::get_flags() const {
    return m_flags;
}

bool storage_entry::has_flags(uint16_t flags) const {
    return (m_flags & flags) == flags;
}

void storage_entry::add_flags(uint16_t flags) {
    m_flags |= flags;
}

void storage_entry::remove_flags(uint16_t flags) {
    m_flags &= ~flags;
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

    add_flags(flag_internal_dirty);
}

storage::storage(listener_storage_ref& listener_storage)
    : m_listener_storage(listener_storage)
    , m_mutex()
    , m_entries()
    , m_paths()
    , m_ids() {
}

entry storage::get_or_create_entry(const std::string_view& path) {
    std::unique_lock guard(m_mutex);

    auto it = m_paths.find(path);
    if (it == m_paths.end()) {
        return create_new_entry(path);
    }

    const auto entry_handle = it->second;
    if (m_entries.has(entry_handle)) {
        // will initialize entry properly
        get_entry_internal(entry_handle);
        return entry_handle;
    }

    // todo: better handling
    throw no_such_handle_exception(entry_handle);
}

void storage::delete_entry(entry entry) {
    std::unique_lock guard(m_mutex);

    delete_entry_internal(entry);
}

void storage::delete_entries(const std::string_view& path) {
    std::unique_lock guard(m_mutex);

    std::vector<entry> handles;
    for (auto [handle, data] : m_entries) {
        if (data.is_in(path)) {
            delete_entry_internal(handle, true, false);
        }
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

    auto data = m_entries[entry];
    return data->get_flags() & ~flag_internal_mask;
}

void storage::get_entry_value(entry entry, value_t& value) {
    std::unique_lock guard(m_mutex);

    auto data = m_entries[entry];
    if (data->has_flags(flag_internal_deleted)) {
        throw entry_deleted_exception();
    }

    data->get_value(value);
}

void storage::set_entry_value(entry entry, const value_t& value) {
    std::unique_lock guard(m_mutex);

    set_entry_internal(entry, value);
}

void storage::clear_entry(entry entry) {
    std::unique_lock guard(m_mutex);

    set_entry_internal(entry, {}, true);
}

void storage::act_on_dirty_entries(const entry_action& action) {
    std::unique_lock guard(m_mutex);

    for (auto [handle, data] : m_entries) {
        if (!data.is_dirty()) {
            continue;
        }

        guard.unlock();
        const auto resume = action(data);
        guard.lock();

        if (resume) {
            data.clear_dirty();
            continue;
        } else {
            break;
        }
    }
}

void storage::clear_net_ids() {
    std::unique_lock guard(m_mutex);

    for (auto [handle, data] : m_entries) {
        data.clear_net_id();
    }
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

void storage::on_entry_created(entry_id id, const std::string& path, const value_t& value) {
    std::unique_lock guard(m_mutex);

    entry entry;
    auto it = m_paths.find(path);
    if (it != m_paths.end()) {
        // entry exists
        entry = it->second;
    } else {
        // entry does not exist
        entry = create_new_entry(path);
    }

    m_ids.emplace(id, entry);
    set_entry_internal(entry, value, false, id, false);
}

void storage::on_entry_updated(entry_id id, const value_t& value) {
    std::unique_lock guard(m_mutex);

    auto it = m_ids.find(id);
    if (it == m_ids.end()) {
        // no such id, what?
        return;
    }

    set_entry_internal(it->second, value, false, id, false);
}

void storage::on_entry_deleted(entry_id id) {
    std::unique_lock guard(m_mutex);

    auto it = m_ids.find(id);
    if (it == m_ids.end()) {
        // no such id, what?
        return;
    }

    delete_entry_internal(it->second, false);
}

void storage::on_entry_id_assigned(entry_id id, const std::string& path) {
    std::unique_lock guard(m_mutex);

    entry entry;
    auto it = m_paths.find(path);
    if (it != m_paths.end()) {
        // entry exists
        entry = it->second;
    } else {
        // entry does not exist
        entry = create_new_entry(path);
    }

    m_ids.emplace(id, entry);
}

entry storage::create_new_entry(const std::string_view& path) {
    auto entry = m_entries.allocate_new_with_handle(path);
    auto data = m_entries[entry];
    data->mark_dirty();

    m_paths.emplace(path, entry);

    m_listener_storage->notify(
            event_type::created,
            data->get_path());

    return entry;
}

storage_entry* storage::get_entry_internal(entry entry, bool mark_dirty) {
    auto data = m_entries[entry];
    if (data->has_flags(flag_internal_deleted)) {
        data->remove_flags(flag_internal_deleted);

        if (mark_dirty) {
            data->mark_dirty();
        }
    }

    return data;
}

void storage::set_entry_internal(entry entry,
                                 const value_t& value,
                                 bool clear,
                                 entry_id id,
                                 bool mark_dirty) {
    // todo: don't set always, base it on timestamp of receive value vs now value

    auto data = get_entry_internal(entry, mark_dirty);

    if (mark_dirty) {
        data->mark_dirty();
    } else {
        data->clear_dirty();
    }

    if (id != id_not_assigned) {
        data->set_net_id(id);
    }

    if (clear) {
        data->clear();
        m_listener_storage->notify(
                event_type::cleared,
                data->get_path());
    } else {
        auto old_value = data->set_value(value);
        m_listener_storage->notify(
                event_type::value_change,
                data->get_path(),
                old_value,
                value);
    }
}

void storage::delete_entry_internal(entry entry, bool mark_dirty, bool notify) {
    // todo: don't set always, base it on timestamp of receive value vs now value

    auto data = get_entry_internal(entry, mark_dirty);

    data->clear();
    data->add_flags(flag_internal_deleted);

    if (mark_dirty) {
        data->mark_dirty();
    } else {
        // deletion overrides anything else, so if server deleted this,
        // clear dirty flag
        data->clear_dirty();
    }

    if (notify) {
        m_listener_storage->notify(
                event_type::deleted,
                data->get_path());
    }
}

}
