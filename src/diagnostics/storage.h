#pragma once

#include <mutex>
#include <map>
#include <atomic>

#include "diagnostics/dispatcher.h"
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
    struct data_snapshot {
        using type = std::map<obsr::handle, entry>;

        data_snapshot();

        std::shared_ptr<const type> read() const;
        type& write();
        void swap();

    private:
        std::atomic<std::shared_ptr<const type>> m_read_data;
        type m_write_data;
    };

    void on_event(const diagnostic_event& event);

    std::mutex m_mutex;
    std::shared_ptr<storage::storage> m_storage;
    event_dispatcher_ptr m_dispatcher;
    data_snapshot m_data;
    std::chrono::milliseconds m_last_sync;
};

}
