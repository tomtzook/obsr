
#include <ranges>

#include "storage.h"

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

storage_monitor::storage_monitor(std::shared_ptr<storage::storage> storage)
    : m_mutex()
    , m_storage(std::move(storage))
    , m_listener(empty_handle)
    , m_data()
    , m_last_sync()
{}

storage_monitor::~storage_monitor() {
    if (m_listener != empty_handle) {
        m_storage->remove_listener(m_listener);
    }
}

std::shared_ptr<const std::map<obsr::handle, storage_monitor::entry>> storage_monitor::get_data_snapshot() const {
    return m_data.read();
}

void storage_monitor::start() {
    std::unique_lock lock(m_mutex);

    auto& snapshot = m_data.write();
    m_storage->foreach_entry([&snapshot](const auto& entry) {
        struct entry our_entry{entry.get_handle(), std::string(entry.get_path()), entry.get_value()};
        snapshot.emplace(our_entry.handle, std::move(our_entry));
    });

    m_listener = m_storage->listen("/", [this](const auto& event)->void {
        on_event(event);
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

void storage_monitor::on_event(const obsr::event& event) {
    std::unique_lock lock(m_mutex);

    auto& snapshot = m_data.write();

    const auto handle = event.get_entry();
    switch (event.get_type()) {
        case event_type::created: {
            const auto it = snapshot.find(handle);
            if (it != snapshot.end()) {
                // todo: this means a problem!!!
                return;
            }

            entry entry{handle, event.get_path(), value()};
            snapshot.emplace(handle, std::move(entry));
            break;
        }
        case event_type::deleted: {
            const auto it = snapshot.find(handle);
            if (it == snapshot.end()) {
                // todo: this means a problem!!!
                return;
            }

            snapshot.erase(it);
            break;
        }
        case event_type::value_changed: {
            const auto it = snapshot.find(handle);
            if (it == snapshot.end()) {
                // todo: this means a problem!!!
                return;
            }

            it->second.value = event.get_value();
            break;
        }
    }
}

}
