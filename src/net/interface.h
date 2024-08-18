#pragma once

#include <cstddef>
#include <mutex>
#include <unordered_map>

#include "obsr_types.h"
#include "io/buffer.h"
#include "util/state.h"

namespace obsr::net {

class net_link {
public:
    virtual void entry_created(obsr::handle handle, const std::string& name, const value_t& value) = 0;
    virtual void entry_updated(obsr::handle handle, const value_t& value) = 0;
    virtual void entry_deleted(obsr::handle handle) = 0;
};

class storage_link {
public:
    virtual void entry_created(const std::string& name, const value_t& value) = 0;
    virtual void entry_created(obsr::handle handle, const value_t& value) = 0;
    virtual void entry_updated(obsr::handle handle, const value_t& value) = 0;
    virtual void entry_deleted(const std::string& name) = 0;
    virtual void entry_deleted(obsr::handle handle) = 0;

    virtual void attach_net_link(net_link* net_link) = 0;
};

class net_storage : public net_link {
public:
    net_storage();

    void set_storage(storage_link* storage_link);

    // from network
    void on_entry_created(entry_id id, const std::string& name, const value_t& value);
    void on_entry_updated(entry_id id, const value_t& value);
    void on_entry_deleted(entry_id id);

    // from storage
    void entry_created(obsr::handle handle, const std::string& name, const value_t& value) override;
    void entry_updated(obsr::handle handle, const value_t& value) override;
    void entry_deleted(obsr::handle handle) override;

private:
    struct entry_data {
        entry_id id = id_not_assigned;
        obsr::handle storage_handle = empty_handle;
        std::string name;

        value_t value;

        bool dirty = true;
        bool server_published = false;
    };

    std::pair<entry_data*, bool> get_or_create_entry(const std::string& name);

    storage_link* m_storage;
    std::mutex m_mutex;

    std::unordered_map<std::string, entry_data> m_entries;
    std::unordered_map<uint32_t, entry_data*> m_entries_by_id;
    std::unordered_map<obsr::handle, entry_data*> m_entries_by_storage_handle;
};

}
