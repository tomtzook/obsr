#pragma once

#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <iterator>

#include "obsr_types.h"
#include "obsr_internal.h"
#include "util/handles.h"
#include "util/time.h"
#include "listener_storage.h"

namespace obsr::storage {

static constexpr uint16_t flag_internal_shift_start = 8;
static constexpr uint16_t flag_internal_mask = static_cast<uint8_t>(-1) << flag_internal_shift_start;

using entry_id = uint16_t;
constexpr entry_id id_not_assigned = static_cast<entry_id>(-1);

enum entry_internal_flag : uint16_t {
    flag_internal_dirty = (1 << flag_internal_shift_start),
    flag_internal_deleted = (1 << (flag_internal_shift_start + 1)),
    flag_internal_created = (1 << (flag_internal_shift_start + 2))
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

    std::chrono::milliseconds get_last_update_timestamp() const;
    void set_last_update_timestamp(std::chrono::milliseconds timestamp);

    void get_value(value& value) const;
    value set_value(const value& value);
    value clear();

private:
    entry m_handle;
    std::string m_path;
    value m_value;

    std::chrono::milliseconds m_last_update_timestamp;
    entry_id m_net_id;
    uint16_t m_flags;
};

class storage {
public:
    using entry_action = std::function<bool(storage_entry&)>;

    explicit storage(listener_storage_ref& listener_storage, const std::shared_ptr<clock>& clock);

    entry get_or_create_entry(const std::string_view& path);
    void delete_entry(entry entry);
    void delete_entries(const std::string_view& path);

    uint32_t probe(entry entry);
    void get_entry_value(entry entry, value& value);
    void set_entry_value(entry entry, const value& value);
    void clear_entry(entry entry);

    void act_on_entries(const entry_action& action, uint16_t required_flags = 0);
    void clear_net_ids();

    listener listen(entry entry, const listener_callback& callback);
    listener listen(const std::string_view& prefix, const listener_callback& callback);
    void remove_listener(listener listener);

    // should be used from network code
    void on_entry_created(entry_id id,
                          std::string_view path,
                          const value& value,
                          std::chrono::milliseconds timestamp);
    void on_entry_updated(entry_id id,
                          const value& value,
                          std::chrono::milliseconds timestamp);
    void on_entry_deleted(entry_id id,
                          std::chrono::milliseconds timestamp);
    void on_entry_id_assigned(entry_id id,
                              std::string_view path);

private:
    entry create_new_entry(const std::string_view& path);

    void set_entry_internal(entry entry,
                            const value& value,
                            bool clear = false,
                            entry_id id = id_not_assigned,
                            bool mark_dirty = true,
                            std::chrono::milliseconds timestamp = std::chrono::milliseconds(0));
    void delete_entry_internal(entry entry,
                               bool mark_dirty = true,
                               bool notify = true,
                               std::chrono::milliseconds timestamp = std::chrono::milliseconds(0));

    listener_storage_ref m_listener_storage;
    std::shared_ptr<clock> m_clock;

    std::mutex m_mutex;
    handle_table<storage_entry, 256> m_entries;
    std::map<std::string, entry, std::less<>> m_paths;
    std::map<entry_id, entry> m_ids;
};

}
