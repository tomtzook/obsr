
#include "internal_except.h"
#include "debug.h"

#include "instance.h"

namespace obsr {

#define LOG_MODULE "instance"

object_data::object_data(const std::string_view& path)
    : path(path) {
}

instance::instance()
    : m_mutex()
    , m_clock(std::make_shared<clock>())
    , m_listener_storage(std::make_shared<storage::listener_storage>())
    , m_storage(std::make_shared<storage::storage>(m_listener_storage, m_clock))
    , m_looper(std::make_shared<events::looper>())
    , m_looper_thread(m_looper)
    , m_net_interface()
    , m_objects()
    , m_object_paths()
    , m_root(m_objects.allocate_new("")) {
}

instance::~instance() {
    stop_network();
}

std::chrono::milliseconds instance::time() {
    return m_clock->now();
}

object instance::get_root() {
    std::unique_lock guard(m_mutex);

    return m_root;
}

object instance::get_child(object obj, std::string_view name) {
    std::unique_lock guard(m_mutex);

    auto data = m_objects[obj];
    const auto path = fmt::format("{}/{}", data->path, name);

    auto it = m_object_paths.find(path);
    if (it == m_object_paths.end()) {
        const auto handle = m_objects.allocate_new(path);
        m_object_paths.emplace(path, handle);

        return handle;
    } else {
        return it->second;
    }
}

entry instance::get_entry(object obj, std::string_view name) {
    std::unique_lock guard(m_mutex);

    auto data = m_objects[obj];
    const auto path = fmt::format("{}/{}", data->path, name);

    return m_storage->get_or_create_entry(path);
}

void instance::delete_object(object obj) {
    std::unique_lock guard(m_mutex);

    if (obj == m_root) {
        throw cannot_delete_root_exception();
    }

    auto data = m_objects[obj];
    m_storage->delete_entries(data->path);

    m_objects.release(obj);
}

void instance::delete_entry(entry entry) {
    std::unique_lock guard(m_mutex);

    m_storage->delete_entry(entry);
}

uint32_t instance::probe(entry entry) {
    std::unique_lock guard(m_mutex);

    return m_storage->probe(entry);
}

obsr::value instance::get_value(entry entry) {
    std::unique_lock guard(m_mutex);

    auto opt = m_storage->get_entry_value(entry);
    if (opt) {
        return std::move(opt.value());
    }

    return value::make();
}

void instance::set_value(entry entry, const obsr::value& value) {
    std::unique_lock guard(m_mutex);

    m_storage->set_entry_value(entry, value);
}

void instance::clear_value(entry entry) {
    std::unique_lock guard(m_mutex);

    m_storage->clear_entry(entry);
}

listener instance::listen_object(object obj, const listener_callback& callback) {
    std::unique_lock guard(m_mutex);

    auto data = m_objects[obj];
    return m_storage->listen(data->path, callback);
}

listener instance::listen_entry(entry entry, const listener_callback& callback) {
    std::unique_lock guard(m_mutex);

    return m_storage->listen(entry, callback);
}

void instance::delete_listener(listener listener) {
    std::unique_lock guard(m_mutex);

    m_storage->remove_listener(listener);
}

void instance::start_server(uint16_t bind_port) {
    std::unique_lock guard(m_mutex);

    if (m_net_interface) {
        throw illegal_state_exception();
    }

    auto network_server = std::make_shared<net::network_server>(m_clock);
    try {
        network_server->configure_bind(bind_port);
        start_net(network_server);
    } catch (const std::exception& e) {
        TRACE_ERROR(LOG_MODULE, "error while starting network server: what=%s", e.what());
        stop_net(network_server);
        throw;
    }

    m_net_interface = network_server;
}

void instance::start_client(std::string_view address, uint16_t server_port) {
    std::unique_lock guard(m_mutex);

    if (m_net_interface) {
        throw illegal_state_exception();
    }

    auto network_client = std::make_shared<net::network_client>(m_clock);
    try {
        network_client->configure_target({std::string(address), server_port});
        start_net(network_client);
    } catch (const std::exception& e) {
        TRACE_ERROR(LOG_MODULE, "error while starting network client: what=%s", e.what());
        stop_net(network_client);
        throw;
    }

    m_net_interface = network_client;
}

void instance::stop_network() {
    std::unique_lock guard(m_mutex);

    if (m_net_interface) {
        stop_net(m_net_interface);
        m_net_interface.reset();
    }
}

void instance::start_net(const std::shared_ptr<net::network_interface>& network_interface) {
    network_interface->attach_storage(m_storage);
    network_interface->start(m_looper.get());
}

void instance::stop_net(const std::shared_ptr<net::network_interface>& network_interface) {
    network_interface->stop();
}

}
