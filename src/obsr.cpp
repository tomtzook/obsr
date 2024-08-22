
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

void get_value(entry entry, obsr::value& value) {
    s_instance.get_value(entry, value);
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
