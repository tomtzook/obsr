#pragma once

#include "obsr_types.h"
#include "obsr_internal.h"
#include "util/handles.h"

namespace obsr::storage {

class storage {
public:
    storage();

    object get_root() const;
    object get_or_create_child(object object, const std::string_view& name);
    entry get_or_create_entry(object object, const std::string_view& name);

    void get_entry_value(entry entry, value& value);
    void set_entry_value(entry entry, const value& value);

private:
    object create_new_object(object_data* parent, const std::string_view& name);
    entry create_new_child(object_data* parent, const std::string_view& name);

    void report_new_object(const object_data* object);
    void report_new_entry(const entry_data* entry);
    void report_entry_value_change(const entry_data* entry, const value& old_value, const value& new_value);

    handle_table<object_data, 256> m_object_handles;
    handle_table<entry_data, 256> m_entry_handles;

    object m_root;
};

}
