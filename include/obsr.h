#pragma once

#include <string>

#include "obsr_types.h"

namespace obsr {

instance create();

object get_root(instance instance);
object get_child(instance instance, object obj, std::string_view name);
entry get_entry(instance instance, object obj, std::string_view name);

void get_value(instance instance, entry entry, value& value);
void set_value(instance instance, entry entry, const value& value);

}
