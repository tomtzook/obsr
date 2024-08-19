#pragma once

#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <iterator>

#include "obsr_types.h"
#include "obsr_internal.h"
#include "util/handles.h"
#include "listener_storage.h"

namespace obsr::storage {

static constexpr uint16_t flag_internal_shift_start = 8;
static constexpr uint16_t flag_internal_mask = static_cast<uint8_t>(-1) << flag_internal_shift_start;

using entry_id = uint16_t;
constexpr entry_id id_not_assigned = static_cast<entry_id>(-1);

enum entry_internal_flag : uint16_t {
    flag_internal_dirty = (1 << flag_internal_shift_start),
    flag_internal_deleted = (1 << (flag_internal_shift_start + 1))
};

struct storage_entry {
    storage_entry(entry handle, const std::string_view& path);

    bool is_in(const std::string_view& path) const;
    std::string_view get_path() const;

    entry_id get_net_id() const;
    void set_net_id(entry_id id);
    void clear_net_id();

    uint16_t get_flags() const;
    bool has_flags(uint16_t flags) const;
    void add_flags(uint16_t flags);
    void remove_flags(uint16_t flags);

    inline bool is_dirty() const {
        return has_flags(flag_internal_dirty);
    }
    inline void mark_dirty() {
        add_flags(flag_internal_dirty);
    }
    void clear_dirty() {
        remove_flags(flag_internal_dirty);
    }

    void get_value(value_t& value) const;
    value_t set_value(const value_t& value);
    void clear();

private:
    entry m_handle;
    std::string m_path;
    value_t m_value;

    entry_id m_net_id;
    uint16_t m_flags;
};

class storage {
public:
    using entry_action = std::function<bool(storage_entry&)>;

    explicit storage(listener_storage_ref& listener_storage);

    entry get_or_create_entry(const std::string_view& path);
    void delete_entry(entry entry);
    void delete_entries(const std::string_view& path);

    uint32_t probe(entry entry);
    void get_entry_value(entry entry, value_t& value);
    void set_entry_value(entry entry, const value_t& value);
    void clear_entry(entry entry);

    void act_on_dirty_entries(const entry_action& action);
    void clear_net_ids();

    listener listen(entry entry, const listener_callback& callback);
    listener listen(const std::string_view& prefix, const listener_callback& callback);
    void remove_listener(listener listener);

    // should be used from network code
    void on_entry_created(entry_id id, const std::string& path, const value_t& value);
    void on_entry_updated(entry_id id, const value_t& value);
    void on_entry_deleted(entry_id id);
    void on_entry_id_assigned(entry_id id, const std::string& path);

private:
    entry create_new_entry(const std::string_view& path);
    storage_entry* get_entry_internal(entry entry, bool mark_dirty = true);

    void set_entry_internal(entry entry,
                            const value_t& value,
                            bool clear = false,
                            entry_id id = id_not_assigned,
                            bool mark_dirty = true);
    void delete_entry_internal(entry entry,
                               bool mark_dirty = true,
                               bool notify = true);

    listener_storage_ref m_listener_storage;

    std::mutex m_mutex;
    handle_table<storage_entry, 256> m_entries;
    std::map<std::string, entry, std::less<>> m_paths;
    std::map<entry_id, entry> m_ids;
};

}
