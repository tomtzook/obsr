
#include "obsr_internal.h"
#include "instance.h"
#include "obsr.h"


namespace obsr {

obsr::instance s_instance;


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

void get_value(entry entry, value_t& value) {
    s_instance.get_value(entry, value);
}

void set_value(entry entry, const value_t& value) {
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

}
