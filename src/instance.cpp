
#include "internal_except.h"
#include "debug.h"

#include "instance.h"

namespace obsr {

#define LOG_MODULE "instance"

static std::optional<std::string> get_parent_path(const std::string& path) {
    const auto index = path.rfind('/');
    if (index == std::string::npos) {
        return std::nullopt;
    }

    return path.substr(0, index);
}

static std::string get_path_name(const std::string& path) {
    const auto index = path.rfind('/');
    if (index == std::string::npos) {
        return "";
    }

    return path.substr(index + 1);
}

static void verify_valid_name(const std::string_view name) {
    if (name.empty()) {
        throw invalid_name_exception(name);
    }

    if (name.find('/') != std::string_view::npos) {
        throw invalid_name_exception(name);
    }
}

object_data::object_data(const std::string_view name, const std::string_view path)
    : name(name)
    , path(path)
{}

instance::instance()
    : m_mutex()
    , m_clock(std::make_shared<clock>())
    , m_listener_storage(std::make_shared<storage::listener_storage>(m_clock))
    , m_storage(std::make_shared<storage::storage>(m_listener_storage, m_clock))
    , m_loop(looper::empty_handle)
    , m_net_agent()
    , m_objects()
    , m_object_paths()
    , m_root(m_objects.allocate_new("", ""))
    , m_diagnostics_server() {
    m_loop = looper::create();
    looper::exec_in_thread(m_loop);
}

instance::~instance() {
    stop_network();
}

std::chrono::milliseconds instance::time() const {
    return m_clock->now();
}

object instance::get_root() {
    std::unique_lock guard(m_mutex);

    return m_root;
}

object instance::get_object(const std::string_view path) {
    std::unique_lock guard(m_mutex);

    if (path.empty()) {
        return m_root;
    }

    return get_or_create_object(path);
}

entry instance::get_entry(const std::string_view path) {
    std::unique_lock guard(m_mutex);

    const std::string path_owned(path);
    const auto ppath_opt = get_parent_path(path_owned);
    if (!ppath_opt) {
        throw invalid_path_exception(path);
    }

    const auto& ppath = ppath_opt.value();
    (void) get_or_create_object(ppath); // create object hierarchy

    return m_storage->get_or_create_entry(path);
}

object instance::get_child(const object obj, const std::string_view name) {
    std::unique_lock guard(m_mutex);

    verify_valid_name(name);

    return get_or_create_child(obj, name);
}

entry instance::get_entry(const object obj, std::string_view name) {
    std::unique_lock guard(m_mutex);

    verify_valid_name(name);

    const auto data = m_objects[obj];
    const auto path = fmt::format("{}/{}", data->path, name);

    return m_storage->get_or_create_entry(path);
}

object instance::get_parent_for_object(const object obj) {
    std::unique_lock guard(m_mutex);

    const auto data = m_objects[obj];
    const auto path_opt = get_parent_path(data->path);
    if (!path_opt) {
        throw no_parent_exception();
    }

    const auto path = path_opt.value();
    if (path.empty()) {
        return m_root;
    }

    // todo: what happens with deleted objects?? we don't delete objects actually
    const auto it = m_object_paths.find(path);
    assert(it != m_object_paths.end());

    return it->second;
}

object instance::get_parent_for_entry(const entry entry) {
    std::unique_lock guard(m_mutex);

    const auto entry_path = m_storage->get_entry_path(entry);
    const auto path_opt = get_parent_path(entry_path);
    assert(path_opt.has_value());

    const auto path = path_opt.value();
    if (path.empty()) {
        return m_root;
    }

    const auto it = m_object_paths.find(path_opt.value());
    assert(it != m_object_paths.end());

    return it->second;
}

std::string instance::get_path_for_object(const object obj) {
    std::unique_lock guard(m_mutex);

    const auto data = m_objects[obj];
    return data->path;
}

std::string instance::get_path_for_entry(const entry entry) {
    std::unique_lock guard(m_mutex);

    return m_storage->get_entry_path(entry);
}

std::string instance::get_name_for_object(const object obj) {
    std::unique_lock guard(m_mutex);

    const auto data = m_objects[obj];
    return data->name;
}

std::string instance::get_name_for_entry(const entry entry) {
    std::unique_lock guard(m_mutex);

    const auto path = m_storage->get_entry_path(entry);
    return get_path_name(path);
}

void instance::delete_object(const object obj) {
    std::unique_lock guard(m_mutex);

    if (obj == m_root) {
        throw cannot_delete_root_exception();
    }

    const auto data = m_objects[obj];
    m_storage->delete_entries(data->path);

    m_objects.release(obj);
}

void instance::delete_entry(const entry entry) {
    std::unique_lock guard(m_mutex);

    m_storage->delete_entry(entry);
}

uint32_t instance::probe(const entry entry) {
    std::unique_lock guard(m_mutex);

    return m_storage->probe(entry);
}

obsr::value instance::get_value(const entry entry) {
    std::unique_lock guard(m_mutex);

    auto opt = m_storage->get_entry_value(entry);
    if (!opt) {
        throw entry_does_not_exist_exception(entry);
    }

    return std::move(opt.value());
}

void instance::set_value(const entry entry, const obsr::value& value) {
    std::unique_lock guard(m_mutex);

    m_storage->set_entry_value(entry, value);
}

void instance::clear_value(const entry entry) {
    std::unique_lock guard(m_mutex);

    m_storage->clear_entry(entry);
}

listener instance::listen_object(const object obj, listener_callback&& callback) {
    std::unique_lock guard(m_mutex);

    const auto data = m_objects[obj];
    return m_storage->listen(data->path, std::move(callback));
}

listener instance::listen_entry(const entry entry, listener_callback&& callback) {
    std::unique_lock guard(m_mutex);

    return m_storage->listen(entry, std::move(callback));
}

void instance::delete_listener(const listener listener) {
    std::unique_lock guard(m_mutex);

    m_storage->remove_listener(listener);
}

void instance::start_server(const uint16_t bind_port) {
    std::unique_lock guard(m_mutex);

    if (!std::holds_alternative<std::monostate>(m_net_agent)) {
        throw illegal_state_exception("network interface already open");
    }

    auto net_agent = std::make_unique<net::server::network_server>(m_clock);
    try {
        net_agent->configure_bind(bind_port);
        net_agent->attach_storage(m_storage);

        if (m_diagnostics_dispatcher) {
            net_agent->attach_diagnostics_dispatcher(m_diagnostics_dispatcher);
        }

        net_agent->start(m_loop);

        m_net_agent = std::move(net_agent);
    } catch (const std::exception& e) {
        TRACE_ERROR(LOG_MODULE, "error while starting network server: what=%s", e.what());
        net_agent->stop();
        throw;
    }
}

void instance::start_client(const std::string_view address, const uint16_t server_port) {
    std::unique_lock guard(m_mutex);

    if (!std::holds_alternative<std::monostate>(m_net_agent)) {
        throw illegal_state_exception("network interface already open");
    }

    auto net_agent = std::make_unique<net::client::network_client>(m_clock);
    try {
        net_agent->configure_target({std::string(address), server_port});
        net_agent->attach_storage(m_storage);

        if (m_diagnostics_dispatcher) {
            net_agent->attach_diagnostics_dispatcher(m_diagnostics_dispatcher);
        }

        net_agent->start(m_loop);

        m_net_agent = std::move(net_agent);
    } catch (const std::exception& e) {
        TRACE_ERROR(LOG_MODULE, "error while starting network client: what=%s", e.what());
        net_agent->stop();
        throw;
    }
}

void instance::stop_network() {
    std::unique_lock guard(m_mutex);

    if (!std::holds_alternative<std::monostate>(m_net_agent)) {
        std::visit([]<typename T0>(T0&& agent)->void {
            using T = std::decay_t<T0>;
            if constexpr (!std::is_same_v<T, std::monostate>) {
                agent->stop();
            }
        }, m_net_agent);

        m_net_agent = std::monostate{};
    }
}

void instance::start_diagnostics(const uint16_t port) {
    std::unique_lock guard(m_mutex);

    m_diagnostics_dispatcher = std::make_shared<diagnostics::event_dispatcher>();
    m_diagnostics_server = std::make_unique<diagnostics::server>(m_storage, m_diagnostics_dispatcher, port);

    m_storage->set_diagnostics_dispatcher(m_diagnostics_dispatcher);

    if (!std::holds_alternative<std::monostate>(m_net_agent)) {
        std::visit([this]<typename T0>(T0&& agent)->void {
            using T = std::decay_t<T0>;
            if constexpr (!std::is_same_v<T, std::monostate>) {
                agent->attach_diagnostics_dispatcher(m_diagnostics_dispatcher);
            }
        }, m_net_agent);
    }
}

void instance::stop_diagnostics() {
    std::unique_lock guard(m_mutex); // todo: potential deadlock with diagnostics code?

    m_storage->set_diagnostics_dispatcher(diagnostics::event_dispatcher_ptr());

    if (!std::holds_alternative<std::monostate>(m_net_agent)) {
        std::visit([]<typename T0>(T0&& agent)->void {
            using T = std::decay_t<T0>;
            if constexpr (!std::is_same_v<T, std::monostate>) {
                agent->attach_diagnostics_dispatcher(diagnostics::event_dispatcher_ptr());
            }
        }, m_net_agent);
    }

    m_diagnostics_dispatcher.reset();
    m_diagnostics_server.reset();
}

object instance::get_or_create_child(const object parent, const std::string_view name) {
    const auto data = m_objects[parent];
    const auto path = fmt::format("{}/{}", data->path, name);

    const auto it = m_object_paths.find(path);
    if (it == m_object_paths.end()) {
        const auto handle = m_objects.allocate_new(name, path);
        m_object_paths.emplace(path, handle);

        return handle;
    } else {
        return it->second;
    }
}

object instance::get_or_create_object(std::string_view path) {
    size_t pos = 0;
    const size_t len = path.length();

    obsr::object current = m_root;
    size_t index;
    do {
        index = path.find('/', pos + 1);
        if (index < len) {
            const auto name = path.substr(pos + 1, index - pos - 1);
            pos = index;

            current = get_or_create_child(current, name);
        } else {
            // this is the leaf
            const auto name = path.substr(pos + 1);
            if (name.empty()) {
                if (current == m_root) {
                    // no child, just root
                    return m_root;
                } else {
                    throw invalid_path_exception(path);
                }
            }

            return get_or_create_child(current, name);
        }
    } while (index < len);

    throw invalid_path_exception(path);
}

}
