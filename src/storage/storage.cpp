
#include "obsr_except.h"
#include "util/time.h"
#include "debug.h"

#include "storage.h"


namespace obsr::storage {

#define LOG_MODULE "storage"

storage_entry::storage_entry(entry handle, const std::string_view& path)
    : m_handle(handle)
    , m_path(path)
    , m_value()
    , m_net_id(id_not_assigned)
    , m_flags(0)
    , m_last_update_timestamp(0) {
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

std::chrono::milliseconds storage_entry::get_last_update_timestamp() const {
    return m_last_update_timestamp;
}

void storage_entry::set_last_update_timestamp(std::chrono::milliseconds timestamp) {
    m_last_update_timestamp = timestamp;
}

void storage_entry::get_value(value& value) const {
    value = m_value;
}

value storage_entry::set_value(const value& value) {
    const auto old_type = m_value.type;
    const auto new_type = value.type;
    if (old_type != value_type::empty && old_type != new_type) {
        throw entry_type_mismatch_exception(m_handle, old_type, new_type);
    }

    auto old = m_value;
    m_value = value;

    return old;
}

value storage_entry::clear() {
    auto old = m_value;
    m_value = value{};

    return old;
}

storage::storage(listener_storage_ref& listener_storage, const std::shared_ptr<clock>& clock)
    : m_listener_storage(listener_storage)
    , m_clock(clock)
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
        return entry_handle;
    }

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

void storage::get_entry_value(entry entry, value& value) {
    std::unique_lock guard(m_mutex);

    auto data = m_entries[entry];
    if (data->has_flags(flag_internal_deleted)) {
        throw entry_deleted_exception();
    }

    data->get_value(value);
}

void storage::set_entry_value(entry entry, const value& value) {
    std::unique_lock guard(m_mutex);

    set_entry_internal(entry, value);
}

void storage::clear_entry(entry entry) {
    std::unique_lock guard(m_mutex);

    set_entry_internal(entry, {}, true);
}

void storage::act_on_entries(const entry_action& action, uint16_t required_flags) {
    std::unique_lock guard(m_mutex);

    for (auto [handle, data] : m_entries) {
        if (required_flags != 0 && !data.has_flags(required_flags)) {
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

bool storage::get_entry_value_from_id(entry_id id, obsr::value& value) {
    std::unique_lock guard(m_mutex);

    auto it = m_ids.find(id);
    if (it == m_ids.end()) {
        // no such id
        return false;
    }

    const auto entry = it->second;
    if (!m_entries.has(entry)) {
        return false;
    }

    auto data = m_entries[entry];
    if (data->has_flags(flag_internal_created) || data->has_flags(flag_internal_deleted)) {
        return false;
    }

    data->get_value(value);
    return true;
}

void storage::on_entry_created(entry_id id,
                               std::string_view path,
                               const value& value,
                               std::chrono::milliseconds timestamp) {
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

    set_entry_internal(entry, value, false, id, false, timestamp);
}

void storage::on_entry_updated(entry_id id,
                               const value& value,
                               std::chrono::milliseconds timestamp) {
    std::unique_lock guard(m_mutex);

    auto it = m_ids.find(id);
    if (it == m_ids.end()) {
        // no such id, what?
        return;
    }

    set_entry_internal(it->second, value, false, id, false, timestamp);
}

void storage::on_entry_deleted(entry_id id, std::chrono::milliseconds timestamp) {
    std::unique_lock guard(m_mutex);

    auto it = m_ids.find(id);
    if (it == m_ids.end()) {
        // no such id, what?
        return;
    }

    delete_entry_internal(it->second, false, true, timestamp);
}

void storage::on_entry_id_assigned(entry_id id,
                                   std::string_view path) {
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

    auto data = m_entries[entry];
    data->set_net_id(id);

    m_ids.emplace(id, entry);
}

entry storage::create_new_entry(const std::string_view& path) {
    auto entry = m_entries.allocate_new_with_handle(path);
    auto data = m_entries[entry];

    m_paths.emplace(path, entry);

    data->add_flags(flag_internal_created);
    data->set_last_update_timestamp(std::chrono::milliseconds(0));

    return entry;
}

void storage::set_entry_internal(entry entry,
                                 const value& value,
                                 bool clear,
                                 entry_id id,
                                 bool mark_dirty,
                                 std::chrono::milliseconds timestamp) {
    auto data = m_entries[entry];

    if (timestamp.count() != 0 && data->get_last_update_timestamp() > timestamp) {
        // this new update is too stale
        TRACE_DEBUG(LOG_MODULE, "received stale set entry request");
        return;
    }

    bool just_created = false;
    if (data->has_flags(flag_internal_created)) {
        TRACE_DEBUG(LOG_MODULE, "received set request on new created entry");

        data->remove_flags(flag_internal_created);
        just_created = true;
    }

    if (data->has_flags(flag_internal_deleted)) {
        TRACE_DEBUG(LOG_MODULE, "received set request on deleted entry");

        data->remove_flags(flag_internal_deleted);
        just_created = true;
    }

    if (just_created) {
        m_listener_storage->notify(
                event_type::created,
                data->get_path());
    }

    if (id != id_not_assigned) {
        data->set_net_id(id);
    }

    obsr::value old_value{};
    if (clear) {
        old_value = data->clear();
    } else {
        old_value = data->set_value(value);
    }

    if (mark_dirty) {
        data->mark_dirty();
    } else {
        data->clear_dirty();
    }

    data->set_last_update_timestamp(m_clock->now());

    m_listener_storage->notify(
            event_type::value_changed,
            data->get_path(),
            old_value,
            value);
}

void storage::delete_entry_internal(entry entry,
                                    bool mark_dirty,
                                    bool notify,
                                    std::chrono::milliseconds timestamp) {
    auto data = m_entries[entry];

    if (timestamp.count() != 0 && data->get_last_update_timestamp() > timestamp) {
        // this new update is too stale
        TRACE_DEBUG(LOG_MODULE, "received stale delete entry request");
        return;
    }

    if (data->has_flags(flag_internal_created) || data->has_flags(flag_internal_deleted)) {
        TRACE_DEBUG(LOG_MODULE, "received delete request on created/deleted entry");
        return;
    }

    data->clear();
    data->add_flags(flag_internal_deleted);

    if (mark_dirty) {
        data->mark_dirty();
    } else {
        // deletion overrides anything else, so if server deleted this,
        // clear dirty flag
        data->clear_dirty();
    }

    data->set_last_update_timestamp(m_clock->now());

    if (notify) {
        m_listener_storage->notify(
                event_type::deleted,
                data->get_path());
    }
}

}
