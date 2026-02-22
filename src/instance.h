#pragma once

#include <mutex>
#include <variant>
#include <fmt/format.h>

#include <looper_cxx.hpp>

#include "obsr_internal.h"
#include "storage/storage.h"
#include "net/client.h"
#include "net/server.h"
#include "util/time.h"
#include "diagnostics/server.h"

namespace obsr {

struct object_data {
    object_data(std::string_view name, std::string_view path);

    std::string name;
    std::string path;
};

class instance {
public:
    instance();
    ~instance();

    instance(const instance&) = delete;
    instance(instance&&) = delete;
    instance& operator=(const instance&) = delete;
    instance& operator=(instance&&) = delete;

    [[nodiscard]] std::chrono::milliseconds time() const;

    [[nodiscard]] object get_root();
    [[nodiscard]] object get_object(std::string_view path);
    [[nodiscard]] entry get_entry(std::string_view path);

    [[nodiscard]] object get_child(object obj, std::string_view name);
    [[nodiscard]] entry get_entry(object obj, std::string_view name);

    [[nodiscard]] object get_parent_for_object(object obj);
    [[nodiscard]] object get_parent_for_entry(entry entry);

    [[nodiscard]] std::string get_path_for_object(object obj);
    [[nodiscard]] std::string get_path_for_entry(entry entry);

    [[nodiscard]] std::string get_name_for_object(object obj);
    [[nodiscard]] std::string get_name_for_entry(entry entry);

    void delete_object(object obj);
    void delete_entry(entry entry);

    [[nodiscard]] uint32_t probe(entry entry);
    [[nodiscard]] obsr::value get_value(entry entry);
    void set_value(entry entry, const obsr::value& value);
    void clear_value(entry entry);

    [[nodiscard]] listener listen_object(object obj, listener_callback&& callback);
    [[nodiscard]] listener listen_entry(entry entry, listener_callback&& callback);
    void delete_listener(listener listener);

    void start_server(uint16_t bind_port);
    void start_client(std::string_view address, uint16_t server_port);
    void stop_network();

    void start_diagnostics();
    void stop_diagnostics();

private:
    [[nodiscard]] object get_or_create_child(object parent, std::string_view name);
    [[nodiscard]] object get_or_create_object(std::string_view path);

    using server_ptr = std::unique_ptr<net::server::network_server>;
    using client_ptr = std::unique_ptr<net::client::network_client>;
    using net_agent_type = std::variant<std::monostate, server_ptr, client_ptr>;

    std::mutex m_mutex;
    clock_ptr m_clock;
    storage::listener_storage_ptr m_listener_storage;
    std::shared_ptr<storage::storage> m_storage;

    looper::loop_holder m_loop;
    net_agent_type m_net_agent;

    handle_table<object_data, 1024> m_objects;
    std::map<std::string, object, std::less<>> m_object_paths;
    object m_root;

    std::unique_ptr<diagnostics::server> m_diagnostics_server;
};

}
