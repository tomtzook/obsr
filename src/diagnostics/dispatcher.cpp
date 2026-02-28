
#include "debug.h"
#include "dispatcher.h"

#include <cstring>

#include "util/time.h"


namespace obsr::diagnostics {

#define LOG_MODULE "diagnostic_event_dispatcher"

event_dispatcher::event_dispatcher()
    : m_thread_loop_run(true)
    , m_mutex()
    , m_has_events()
    , m_listeners()
    , m_pending_events()
    , m_thread(&event_dispatcher::thread_main, this)
{}

event_dispatcher::~event_dispatcher() {
    m_thread_loop_run.store(false);
    m_has_events.notify_all();
    m_thread.join();
}

void event_dispatcher::listen(listener_callback&& callback) {
    std::unique_lock lock(m_mutex);
    m_listeners.emplace_back(std::move(callback));
}

void event_dispatcher::notify(diagnostic_event&& event) {
    m_pending_events.push(std::move(event));
    m_has_events.notify_one();
}

void event_dispatcher::thread_main() {
    while (m_thread_loop_run.load()) {
        std::unique_lock lock(m_mutex);

        m_has_events.wait(lock);

        while (m_thread_loop_run.load()) {
            const auto opt = m_pending_events.pop();
            if (!opt) {
                break;
            }

            const auto& event = opt.value();
            for (const auto& listener : m_listeners) {
                lock.unlock();
                try {
                    listener(event);
                } catch (const std::exception& e) {
                    TRACE_ERROR(LOG_MODULE, "Error in diagnostic listener callback: what=%s", e.what());
                } catch (...) {
                    TRACE_ERROR(LOG_MODULE, "Error in diagnostic listener callback: unknown");
                }
                lock.lock();
            }
        }
    }
}

void notify_entry_created(const event_dispatcher_ptr& dispatcher, const obsr::handle handle, const std::string_view path) {
    if (!dispatcher) {
        return;
    }

    storage_entry_created data{};
    data.handle = handle;
    strncpy(data.path, path.data(), sizeof(data.path));

    diagnostic_event event;
    event.set(std::move(data));
    dispatcher->notify(std::move(event));
}

void notify_entry_deleted(const event_dispatcher_ptr& dispatcher, const obsr::handle handle) {
    if (!dispatcher) {
        return;
    }

    diagnostic_event event;
    event.set(storage_entry_deleted{ handle });
    dispatcher->notify(std::move(event));
}

void notify_entry_value_set(const event_dispatcher_ptr& dispatcher, const obsr::handle handle, const obsr::value& value) {
    if (!dispatcher) {
        return;
    }

    diagnostic_event event;
    event.set(storage_entry_value_changed{ handle, value });
    dispatcher->notify(std::move(event));
}

void notify_entry_value_clear(const event_dispatcher_ptr& dispatcher, const obsr::handle handle) {
    if (!dispatcher) {
        return;
    }

    diagnostic_event event;
    event.set(storage_entry_value_changed{ handle, value() });
    dispatcher->notify(std::move(event));
}

void notify_entry_flags_changed(const event_dispatcher_ptr& dispatcher, const obsr::handle handle, const uint16_t flags) {
    if (!dispatcher) {
        return;
    }

    diagnostic_event event;
    event.set(storage_entry_flags_changed{ handle, flags });
    dispatcher->notify(std::move(event));
}

void notify_entry_net_id_set(const event_dispatcher_ptr& dispatcher, const obsr::handle handle, const uint16_t id) {
    if (!dispatcher) {
        return;
    }

    diagnostic_event event;
    event.set(storage_entry_net_id_changed{ handle, id });
    dispatcher->notify(std::move(event));
}

void notify_out_network_message(
    const event_dispatcher_ptr& dispatcher,
    const uint16_t client_id,
    const uint64_t message_id,
    const net::out_message& message) {
    if (!dispatcher) {
        return;
    }

    network_message data{network_message::data_direction::out, message.type(), client_id, message_id};
    data.timestamp = time_now();

    switch (data.type) {
        case net::message_type::entry_create:
            data.data.entry_create.id = message.id();
            break;
        case net::message_type::entry_update:
            data.data.entry_update.id = message.id();
            break;
        case net::message_type::entry_delete:
            data.data.entry_delete.id = message.id();
            break;
        case net::message_type::entry_id_assign:
            data.data.entry_id_assign.id = message.id();
            break;
        case net::message_type::no_type:
        case net::message_type::handshake_finished:
        case net::message_type::handshake_ready:
        case net::message_type::time_sync_request:
        case net::message_type::time_sync_response:
        default:
            break;
    }

    diagnostic_event event;
    event.set(std::move(data));
    dispatcher->notify(std::move(event));
}

void notify_in_network_message(
    const event_dispatcher_ptr& dispatcher,
    const uint16_t client_id,
    const uint64_t message_id,
    const net::message_type type,
    const net::parse_data& parse_data) {
    if (!dispatcher) {
        return;
    }

    network_message data{network_message::data_direction::in, type, client_id, message_id};
    switch (data.type) {
        case net::message_type::entry_create:
            data.data.entry_create.id = parse_data.id;
            break;
        case net::message_type::entry_update:
            data.data.entry_update.id = parse_data.id;
            break;
        case net::message_type::entry_delete:
            data.data.entry_delete.id = parse_data.id;
            break;
        case net::message_type::entry_id_assign:
            data.data.entry_id_assign.id = parse_data.id;
            break;
        case net::message_type::no_type:
        case net::message_type::handshake_finished:
        case net::message_type::handshake_ready:
        case net::message_type::time_sync_request:
        case net::message_type::time_sync_response:
        default:
            break;
    }

    diagnostic_event event;
    event.set(std::move(data));
    dispatcher->notify(std::move(event));
}

void notify_new_connection(const event_dispatcher_ptr& dispatcher, const uint16_t client_id, net::connection_info addr) {
    if (!dispatcher) {
        return;
    }

    diagnostic_event event;
    event.set(network_connection{client_id, std::move(addr)});
    dispatcher->notify(std::move(event));
}

void notify_new_disconnection(const event_dispatcher_ptr& dispatcher, const uint16_t client_id) {
    if (!dispatcher) {
        return;
    }

    diagnostic_event event;
    event.set(network_disconnection{client_id});
    dispatcher->notify(std::move(event));
}

}
