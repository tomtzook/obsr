#pragma once

#include <mutex>
#include <fmt/format.h>

#include "obsr_internal.h"
#include "storage/storage.h"
#include "net/client.h"
#include "net/server.h"
#include "io/nio.h"
#include "updater.h"

namespace obsr {

struct object_data {
    explicit object_data(const std::string_view& path);

    std::string path;
};

struct instance {
public:
    instance();
    ~instance();

    object get_root();

    object get_child(object obj, std::string_view name);
    entry get_entry(object obj, std::string_view name);

    void delete_object(object obj);
    void delete_entry(entry entry);

    uint32_t probe(entry entry);
    void get_value(entry entry, value_t& value);
    void set_value(entry entry, const value_t& value);
    void clear_value(entry entry);

    listener listen_object(object obj, const listener_callback& callback);
    listener listen_entry(entry entry, const listener_callback& callback);
    void delete_listener(listener listener);

    void start_server(uint16_t bind_port);
    void start_client(std::string_view address, uint16_t server_port);
    void stop_network();

private:
    void configure_net(std::shared_ptr<net::network_interface> network_interface);
    void unconfigure_net(std::shared_ptr<net::network_interface> network_interface);

    std::mutex m_mutex;
    updater m_updater;
    std::shared_ptr<io::nio_runner> m_nio_runner;
    storage::listener_storage_ref m_listener_storage;
    std::shared_ptr<storage::storage> m_storage;

    std::shared_ptr<net::network_interface> m_net_interface;
    obsr::handle m_net_update_handle;

    handle_table<object_data, 256> m_objects;
    std::map<std::string, object, std::less<>> m_object_paths;
    object m_root;
};

}
