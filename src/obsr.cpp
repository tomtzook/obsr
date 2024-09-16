
#include "instance.h"
#include "obsr.h"

namespace obsr {

obsr::instance s_instance;

std::chrono::milliseconds time() {
    return s_instance.time();
}

object get_root() {
    return s_instance.get_root();
}

object get_child(object obj, std::string_view name) {
    return s_instance.get_child(obj, name);
}

entry get_entry(object obj, std::string_view name) {
    return s_instance.get_entry(obj, name);
}

void delete_object(object obj) {
    s_instance.delete_object(obj);
}

void delete_entry(entry entry) {
    s_instance.delete_entry(entry);
}

uint32_t probe(entry entry) {
    return s_instance.probe(entry);
}

obsr::value get_value(entry entry) {
    return s_instance.get_value(entry);
}

void set_value(entry entry, const obsr::value& value) {
    s_instance.set_value(entry, value);
}

void clear_value(entry entry) {
    s_instance.clear_value(entry);
}

listener listen_object(object obj, const listener_callback&& callback) {
    return s_instance.listen_object(obj, callback);
}

listener listen_entry(entry entry, const listener_callback&& callback) {
    return s_instance.listen_entry(entry, callback);
}

void delete_listener(listener listener) {
    s_instance.delete_listener(listener);
}

void start_server(uint16_t bind_port) {
    s_instance.start_server(bind_port);
}

void start_client(std::string_view address, uint16_t server_port) {
    s_instance.start_client(address, server_port);
}

void stop_network() {
    s_instance.stop_network();
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

std::ostream& operator<<(std::ostream& os, obsr::value_type type) {
    switch (type) {
        case obsr::value_type::empty:
            os << "empty";
            break;
        case obsr::value_type::raw:
            os << "raw";
            break;
        case obsr::value_type::boolean:
            os << "bool";
            break;
        case obsr::value_type::integer32:
            os << "int32";
            break;
        case obsr::value_type::integer64:
            os << "int64";
            break;
        case obsr::value_type::floating_point32:
            os << "float";
            break;
        case obsr::value_type::floating_point64:
            os << "double";
            break;
        case obsr::value_type::integer32_array:
            os << "int32_arr";
            break;
        case obsr::value_type::integer64_array:
            os << "int64_arr";
            break;
        case obsr::value_type::floating_point32_array:
            os << "float_arr";
            break;
        case obsr::value_type::floating_point64_array:
            os << "double_arr";
            break;
    }

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
