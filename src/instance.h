#pragma once

#include <mutex>
#include <fmt/format.h>

#include "obsr_internal.h"
#include "storage/storage.h"
#include "net/client.h"
#include "net/server.h"
#include "util/time.h"
#include "events/events.h"

namespace obsr {

struct object_data {
    object_data(std::string_view name, std::string_view path);

    std::string name;
    std::string path;
};

struct instance {
public:
    instance();
    ~instance();

    std::chrono::milliseconds time();

    object get_root();
    object get_object(std::string_view path);
    entry get_entry(std::string_view path);

    object get_child(object obj, std::string_view name);
    entry get_entry(object obj, std::string_view name);

    object get_parent_for_object(object obj);
    object get_parent_for_entry(entry entry);

    std::string get_path_for_object(object obj);
    std::string get_path_for_entry(entry entry);

    std::string get_name_for_object(object obj);
    std::string get_name_for_entry(entry entry);

    void delete_object(object obj);
    void delete_entry(entry entry);

    uint32_t probe(entry entry);
    obsr::value get_value(entry entry);
    void set_value(entry entry, const obsr::value& value);
    void clear_value(entry entry);

    listener listen_object(object obj, const listener_callback& callback);
    listener listen_entry(entry entry, const listener_callback& callback);
    void delete_listener(listener listener);

    void start_server(uint16_t bind_port);
    void start_client(std::string_view address, uint16_t server_port);
    void stop_network();

private:
    void start_net(const std::shared_ptr<net::network_interface>& network_interface);
    void stop_net(const std::shared_ptr<net::network_interface>& network_interface);

    object get_or_create_child(object parent, std::string_view name);
    object get_or_create_object(std::string_view path);

    std::mutex m_mutex;
    std::shared_ptr<clock> m_clock;
    storage::listener_storage_ref m_listener_storage;
    std::shared_ptr<storage::storage> m_storage;

    std::shared_ptr<events::looper> m_looper;
    events::looper_thread m_looper_thread;

    std::shared_ptr<net::network_interface> m_net_interface;

    handle_table<object_data, 256> m_objects;
    std::map<std::string, object, std::less<>> m_object_paths;
    object m_root;
};

}
