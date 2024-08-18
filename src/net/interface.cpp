
#include "debug.h"
#include "util/general.h"

#include "interface.h"

namespace obsr::net {

#define LOG_MODULE "netstorage"

net_storage::net_storage()
    : m_storage(nullptr)
    , m_mutex()
    , m_entries()
    , m_entries_by_storage_handle()
    , m_entries_by_id()
{}

void net_storage::set_storage(storage_link* storage_link) {
    std::unique_lock lock(m_mutex);
    m_storage = storage_link;
}

void net_storage::on_entry_created(entry_id id, const std::string& name, const value_t& value) {
    std::unique_lock lock(m_mutex);

    TRACE_DEBUG(LOG_MODULE, "new entry created %d, name=%s", id, name.c_str());

    auto [entry_data, created] = get_or_create_entry(name);

    entry_data->id = id;
    entry_data->value = value;
    entry_data->dirty = false;
    entry_data->server_published = true;

    m_entries_by_id.emplace(id, entry_data);

    if (entry_data->storage_handle != empty_handle) {
        invoke_ptr<storage_link, obsr::handle, const value_t&>(
                lock,
                m_storage,
                &storage_link::entry_created,
                entry_data->storage_handle,
                value);
    } else {
        invoke_ptr<storage_link, const std::string&, const value_t&>(
                lock,
                m_storage,
                &storage_link::entry_created,
                entry_data->name,
                value);
    }
}

void net_storage::on_entry_updated(entry_id id, const value_t& value) {
    std::unique_lock lock(m_mutex);

    TRACE_DEBUG(LOG_MODULE, "entry updated %d", id);

    auto it = m_entries_by_id.find(id);
    if (it == m_entries_by_id.end()) {
        // entry does not exist
        // todo: what to do?
        return;
    }

    // todo: accept values by timestamps, not any value
    auto entry_data = it->second;
    entry_data->value = value;
    entry_data->dirty = false;

    if (entry_data->storage_handle == empty_handle) {
        invoke_ptr<storage_link, const std::string&, const value_t&>(
                lock,
                m_storage,
                &storage_link::entry_created,
                entry_data->name,
                value);
    } else {
        invoke_ptr<storage_link, obsr::handle, const value_t&>(
                lock,
                m_storage,
                &storage_link::entry_updated,
                entry_data->storage_handle,
                value);
    }
}

void net_storage::on_entry_deleted(entry_id id) {
    std::unique_lock lock(m_mutex);

    TRACE_DEBUG(LOG_MODULE, "entry deleted %d", id);

    auto it = m_entries_by_id.find(id);
    if (it == m_entries_by_id.end()) {
        // entry does not exist
        // todo: what to do?
        return;
    }

    // todo: accept values by timestamps, not any value
    auto entry_data = it->second;
    entry_data->id = id_not_assigned;
    entry_data->server_published = false;

    m_entries_by_id.erase(id);

    if (entry_data->storage_handle == empty_handle) {
        invoke_ptr<storage_link, const std::string&>(
                lock,
                m_storage,
                &storage_link::entry_deleted,
                entry_data->name);
    } else {
        invoke_ptr<storage_link, obsr::handle>(
                lock,
                m_storage,
                &storage_link::entry_deleted,
                entry_data->storage_handle);
    }
}

void net_storage::entry_created(obsr::handle handle, const std::string& name, const value_t& value) {
    std::unique_lock lock(m_mutex);

    entry_data* entry_data;
    auto it = m_entries_by_storage_handle.find(handle);
    if (it == m_entries_by_storage_handle.end()) {
        auto [entry, created] = get_or_create_entry(name);
        entry->storage_handle = handle;

        entry_data = entry;
        m_entries_by_storage_handle.emplace(handle, entry_data);
    } else {
        entry_data = it->second;
    }

    entry_data->value = value;
    entry_data->dirty = true;
}

void net_storage::entry_updated(obsr::handle handle, const value_t& value) {
    std::unique_lock lock(m_mutex);

    auto it = m_entries_by_storage_handle.find(handle);
    if (it == m_entries_by_storage_handle.end()) {
        // todo: what? throw?
        return;
    }

    auto entry_data = it->second;
    entry_data->value = value;
    entry_data->dirty = true;
}

void net_storage::entry_deleted(obsr::handle handle) {
    std::unique_lock lock(m_mutex);

    auto it = m_entries_by_storage_handle.find(handle);
    if (it == m_entries_by_storage_handle.end()) {
        // todo: what? throw?
        return;
    }

    auto entry_data = it->second;
    // todo: how to do delete?
}

std::pair<net_storage::entry_data*, bool> net_storage::get_or_create_entry(const std::string& name) {
    bool created = false;
    auto it = m_entries.find(name);
    if (it == m_entries.end()) {
        created = true;
        auto [it2, inserted] = m_entries.emplace(name, entry_data{});
        // todo: check inserted?

        auto& data = it2->second;
        data.id = id_not_assigned;
        data.name = name;
        data.storage_handle = empty_handle;
        data.dirty = false;
        data.server_published = false;
    }

    return {&it->second, created};
}

}
