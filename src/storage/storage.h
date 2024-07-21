#pragma once

#include <map>
#include <vector>
#include <string>
#include <mutex>

#include "obsr_types.h"
#include "obsr_internal.h"
#include "util/handles.h"
#include "listener_storage.h"

namespace obsr::storage {

struct storage_entry {
    storage_entry(entry handle, const std::string_view& path);

    bool is_in(const std::string_view& path) const;

    std::string_view get_path() const;

    void get_value(value_t& value) const;
    value_t set_value(const value_t& value);
    void clear();

private:
    entry m_handle;
    std::string m_path;
    value_t m_value;
};

class storage {
public:
    explicit storage(listener_storage_ref& listener_storage);

    entry get_or_create_entry(const std::string_view& path);
    void delete_entry(entry entry);
    void delete_entries(const std::string_view& path);

    uint32_t probe(entry entry);
    void get_entry_value(entry entry, value_t& value);
    void set_entry_value(entry entry, const value_t& value);
    void clear_entry(entry entry);

    listener listen(entry entry, const listener_callback& callback);
    listener listen(const std::string_view& prefix, const listener_callback& callback);
    void remove_listener(listener listener);

private:
    entry create_new_entry(const std::string_view& path);

    listener_storage_ref m_listener_storage;

    std::mutex m_mutex;
    handle_table<storage_entry, 256> m_entries;
    std::map<std::string, entry, std::less<>> m_paths;
};

}
