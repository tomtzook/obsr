#pragma once

#include <map>
#include <string>
#include <mutex>
#include <optional>

#include "obsr_types.h"
#include "obsr_internal.h"
#include "util/handles.h"
#include "util/time.h"
#include "listener_storage.h"
#include "diagnostics/dispatcher.h"

namespace obsr::storage {

static constexpr uint16_t flag_internal_shift_start = 8;
static constexpr uint16_t flag_internal_mask = static_cast<uint8_t>(-1) << flag_internal_shift_start;

enum entry_internal_flag : uint16_t {
    flag_internal_dirty = (1 << flag_internal_shift_start),
    flag_internal_deleted = (1 << (flag_internal_shift_start + 1)),
    flag_internal_created = (1 << (flag_internal_shift_start + 2))
};

struct storage_entry {
    storage_entry(entry handle, const std::string_view& path);

    [[nodiscard]] obsr::entry get_handle() const;
    [[nodiscard]] bool is_in(const std::string_view& path) const;
    [[nodiscard]] std::string_view get_path() const;

    [[nodiscard]] entry_id get_net_id() const;
    bool set_net_id(entry_id id);
    void clear_net_id();

    [[nodiscard]] uint16_t get_flags() const;
    [[nodiscard]] bool has_flags(uint16_t flags) const;
    void add_flags(uint16_t flags);
    void remove_flags(uint16_t flags);

    [[nodiscard]] bool is_dirty() const;
    void mark_dirty();
    void clear_dirty();

    [[nodiscard]] std::chrono::milliseconds get_last_update_timestamp() const;
    void set_last_update_timestamp(std::chrono::milliseconds timestamp);

    [[nodiscard]] const value& get_value() const;
    [[nodiscard]] std::optional<value> set_value(const value& value);
    [[nodiscard]] std::optional<value> clear();

private:
    const entry m_handle;
    const std::string m_path;

    value m_value;
    std::chrono::milliseconds m_last_update_timestamp;
    entry_id m_net_id;
    uint16_t m_flags;
};

class storage {
public:
    using entry_view = std::function<void(const storage_entry&)>;
    using entry_action = std::function<bool(const storage_entry&)>;

    explicit storage(listener_storage_ptr listener_storage, clock_ptr clock);

    void foreach_entry(const entry_view& action);
    void foreach_entry(const std::function<void(entry)>&& callback);

    void set_diagnostics_dispatcher(diagnostics::event_dispatcher_ptr dispatcher);

    [[nodiscard]] entry get_or_create_entry(const std::string_view& path);
    void delete_entry(entry entry);
    void delete_entries(const std::string_view& path);

    [[nodiscard]] uint32_t probe(entry entry);
    [[nodiscard]] std::string get_entry_path(entry entry);
    [[nodiscard]] std::optional<obsr::value> get_entry_value(entry entry);
    void set_entry_value(entry entry, const obsr::value& value);
    void clear_entry(entry entry);

    void act_on_dirty_entries(const entry_action& action);
    void clear_net_ids();

    [[nodiscard]] listener listen(entry entry, listener_callback&& callback);
    [[nodiscard]] listener listen(const std::string_view& prefix, listener_callback&& callback);
    void remove_listener(listener listener);

    // should be used from network code
    [[nodiscard]] std::optional<obsr::value> get_entry_value_from_id(entry_id id);
    void on_clock_resync();

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
    [[nodiscard]] entry create_new_entry(const std::string_view& path);

    void set_entry_internal(entry entry,
                            const value& value,
                            bool clear = false,
                            entry_id id = id_not_assigned,
                            bool mark_dirty = true,
                            std::chrono::milliseconds timestamp = std::chrono::milliseconds(0));
    void delete_entry_internal(entry entry,
                               bool mark_dirty = true,
                               std::chrono::milliseconds timestamp = std::chrono::milliseconds(0));

    listener_storage_ptr m_listener_storage;
    diagnostics::event_dispatcher_ptr m_diagnostic_dispatcher;
    clock_ptr m_clock;

    std::recursive_mutex m_mutex; // todo: switch to regular
    handle_table<storage_entry, 1024> m_entries;
    std::map<std::string, entry, std::less<>> m_paths;
    std::map<entry_id, entry> m_ids;
};

}
