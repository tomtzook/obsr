
#include <ranges>

#include "storage.h"

#include <cstring>

namespace obsr::diagnostics {

storage_monitor::data_snapshot::data_snapshot()
    : m_read_data(std::make_shared<const type>())
{}

std::shared_ptr<const storage_monitor::data_snapshot::type> storage_monitor::data_snapshot::read() const {
    return m_read_data.load();
}

storage_monitor::data_snapshot::type& storage_monitor::data_snapshot::write() {
    return m_write_data;
}

void storage_monitor::data_snapshot::swap() {
    // create copy to update the read data
    auto new_data = std::make_shared<const type>(m_write_data);
    m_read_data.store(std::move(new_data));
}

storage_monitor::storage_monitor(std::shared_ptr<storage::storage> storage, event_dispatcher_ptr dispatcher)
    : m_mutex()
    , m_storage(std::move(storage))
    , m_dispatcher(std::move(dispatcher))
    , m_data()
    , m_last_sync()
{}

storage_monitor::~storage_monitor() {

}

std::shared_ptr<const std::map<obsr::handle, storage_monitor::entry>> storage_monitor::get_data_snapshot() const {
    return m_data.read();
}

void storage_monitor::start() {
    std::unique_lock lock(m_mutex);

    /*m_listener = m_storage->listen("/", [this](const auto& event)->void {
        on_event(event);
    });*/

    // todo: need to detach listener!
    m_dispatcher->listen([this](const auto& event)->void {
        on_event(event);
    });

    auto& snapshot = m_data.write();
    m_storage->foreach_entry([&snapshot](const storage::storage_entry& entry) {
        struct entry our_entry{};
        our_entry.handle = entry.get_handle();
        strncpy(our_entry.path, entry.get_path().data(), sizeof(our_entry.path));
        our_entry.flags = entry.get_flags();
        our_entry.net_id = entry.get_net_id();
        our_entry.value = entry.get_value();
        snapshot.emplace(our_entry.handle, std::move(our_entry));
    });

    m_last_sync = time_now();
}

void storage_monitor::sync() {
    std::unique_lock lock(m_mutex);
    
    if (const auto now = time_now(); now - m_last_sync >= std::chrono::milliseconds(100)) {
        m_last_sync = now;
        m_data.swap();
    }
}

void storage_monitor::on_event(const diagnostic_event& event) {
    std::unique_lock lock(m_mutex);

    auto& snapshot = m_data.write();

    if (event.has<storage_entry_created>()) {
        const auto& data = event.get<storage_entry_created>();

        const auto it = snapshot.find(data.handle);
        if (it != snapshot.end()) {
            // todo: this means a problem!!!
            return;
        }

        entry entry{};
        entry.handle = data.handle;
        strncpy(entry.path, data.path, std::min(sizeof(entry.path), sizeof(data.path)));

        snapshot.emplace(data.handle, std::move(entry));
    } else if (event.has<storage_entry_deleted>()) {
        const auto& data = event.get<storage_entry_deleted>();

        const auto it = snapshot.find(data.handle);
        if (it != snapshot.end()) {
            // todo: this means a problem!!!
            return;
        }

        snapshot.erase(it);
    } else if (event.has<storage_entry_value_changed>()) {
        const auto& data = event.get<storage_entry_value_changed>();

        const auto it = snapshot.find(data.handle);
        if (it == snapshot.end()) {
            // todo: this means a problem!!!
            return;
        }

        it->second.value = data.value;
    } else if (event.has<storage_entry_flags_changed>()) {
        const auto& data = event.get<storage_entry_flags_changed>();

        const auto it = snapshot.find(data.handle);
        if (it == snapshot.end()) {
            // todo: this means a problem!!!
            return;
        }

        it->second.flags = data.flags;
    } else if (event.has<storage_entry_net_id_changed>()) {
        const auto& data = event.get<storage_entry_net_id_changed>();

        const auto it = snapshot.find(data.handle);
        if (it == snapshot.end()) {
            // todo: this means a problem!!!
            return;
        }

        it->second.net_id = data.net_id;
    }
}

}
