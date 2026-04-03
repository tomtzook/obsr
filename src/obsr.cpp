
#include "instance.h"
#include "global.h"
#include "obsr.h"

namespace obsr {

std::chrono::milliseconds time() {
    return global_instance().time();
}

object get_root() {
    return global_instance().get_root();
}

object get_object(const std::string_view path) {
    return global_instance().get_object(path);
}

entry get_entry(const std::string_view path) {
    return global_instance().get_entry(path);
}

object get_child(const object obj, const std::string_view name) {
    return global_instance().get_child(obj, name);
}

entry get_entry(const object obj, const std::string_view name) {
    return global_instance().get_entry(obj, name);
}

object get_parent_for_object(const object obj) {
    return global_instance().get_parent_for_object(obj);
}

object get_parent_for_entry(const entry entry) {
    return global_instance().get_parent_for_entry(entry);
}

std::string get_path_for_object(const object obj) {
    return global_instance().get_path_for_object(obj);
}

std::string get_path_for_entry(const entry entry) {
    return global_instance().get_path_for_entry(entry);
}

std::string get_name_for_object(const object obj) {
    return global_instance().get_name_for_object(obj);
}

std::string get_name_for_entry(const entry entry) {
    return global_instance().get_name_for_entry(entry);
}

void foreach_entry(std::function<void(entry)>&& callback) {
    global_instance().foreach_entry(std::move(callback));
}

void delete_object(const object obj) {
    global_instance().delete_object(obj);
}

void delete_entry(const entry entry) {
    global_instance().delete_entry(entry);
}

uint32_t probe(const entry entry) {
    return global_instance().probe(entry);
}

obsr::value get_value(const entry entry) {
    return global_instance().get_value(entry);
}

void set_value(const entry entry, const obsr::value& value) {
    global_instance().set_value(entry, value);
}

void clear_value(const entry entry) {
    global_instance().clear_value(entry);
}

listener listen_object(const object obj, listener_callback&& callback) {
    return global_instance().listen_object(obj, std::move(callback));
}

listener listen_entry(const entry entry, listener_callback&& callback) {
    return global_instance().listen_entry(entry, std::move(callback));
}

void delete_listener(const listener listener) {
    global_instance().delete_listener(listener);
}

void start_server(const uint16_t bind_port) {
    global_instance().start_server(bind_port);
}

void start_client(const std::string_view address, const uint16_t server_port) {
    global_instance().start_client(address, server_port);
}

void stop_network() {
    global_instance().stop_network();
}

void start_diagnostics(const uint16_t port) {
    global_instance().start_diagnostics(port);
}

void stop_diagnostics() {
    global_instance().stop_diagnostics();
}

}

template<typename t_>
std::ostream& operator<<(std::ostream& os, std::span<const t_> arr) {
    os << "[";

    for (int i = 0; i < arr.size(); ++i) {
        if (i > 0) {
            os << ',';
        }

        os << arr.data()[i];
    }

    os << "]";

    return os;
}

std::ostream& operator<<(std::ostream& os, const obsr::value_type type) {
    os << obsr::value_type_str(type);
    return os;
}

std::ostream& operator<<(std::ostream& os, const obsr::value& value) {
    switch (value.get_type()) {
        case obsr::value_type::empty:
            break;
        case obsr::value_type::raw: {
            const auto data = value.get_raw();
            os << "raw(ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(data.data()) << ", size=" << data.size() << ")";
            break;
        }
        case obsr::value_type::string:
            os << value.get_string();
            break;
        case obsr::value_type::boolean:
            os << (value.get_boolean() ? "True" : "False");
            break;
        case obsr::value_type::integer32:
            os << value.get_int32();
            break;
        case obsr::value_type::integer64:
            os << value.get_int64();
            break;
        case obsr::value_type::floating_point32:
            os << value.get_float();
            break;
        case obsr::value_type::floating_point64:
            os << value.get_double();
            break;
        case obsr::value_type::integer32_array:
            os << value.get_int32_array();
            break;
        case obsr::value_type::integer64_array:
            os << value.get_int64_array();
            break;
        case obsr::value_type::floating_point32_array:
            os << value.get_float_array();
            break;
        case obsr::value_type::floating_point64_array:
            os << value.get_double_array();
            break;
    }

    return os;
}

std::ostream& operator<<(std::ostream& os, const obsr::event_type type) {
    switch (type) {
        case obsr::event_type::created:
            os << "created";
            break;
        case obsr::event_type::deleted:
            os << "deleted";
            break;
        case obsr::event_type::value_changed:
            os << "value_changed";
            break;
    }

    return os;
}

std::ostream& operator<<(std::ostream& os, const obsr::event& event) {
    os << "event type=" << event.get_type()
        << ", at=" << event.get_timestamp().count()
        << ", path=" << event.get_path();

    return os;
}
