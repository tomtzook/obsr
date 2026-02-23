#pragma once

#include <mutex>
#include <map>

#include "diagnostics/dispatcher.h"
#include "diagnostics/data.h"
#include "storage/storage.h"

namespace obsr::diagnostics {

class storage_monitor {
public:
    struct entry {
        obsr::handle handle;
        char path[256];
        value value;
        uint16_t net_id;
        uint16_t flags;
    };

    explicit storage_monitor(std::shared_ptr<storage::storage> storage, event_dispatcher_ptr dispatcher);
    ~storage_monitor();

    std::shared_ptr<const std::map<obsr::handle, entry>> get_data_snapshot() const;

    void start();
    void sync();

private:
    void on_event(const diagnostic_event& event);

    std::mutex m_mutex;
    std::shared_ptr<storage::storage> m_storage;
    event_dispatcher_ptr m_dispatcher;
    rw_double_buffer<std::map<obsr::handle, entry>> m_data;
    std::chrono::milliseconds m_last_sync;
};

}
