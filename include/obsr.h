#pragma once

#include <string>
#include <functional>

#include "obsr_types.h"

namespace obsr {

instance create();

object get_root(instance instance);
object get_child(instance instance, object obj, std::string_view name);
entry get_entry(instance instance, object obj, std::string_view name);

void delete_object(instance instance, object obj);
void delete_entry(instance instance, entry entry);

uint32_t probe(instance instance, entry entry);
void get_value(instance instance, entry entry, value_t& value);
void set_value(instance instance, entry entry, const value_t& value);

listener listen_object(instance instance, object obj, const listener_callback&& callback);
listener listen_entry(instance instance, entry entry, const listener_callback&& callback);
void delete_listener(instance instance, listener listener);

}
