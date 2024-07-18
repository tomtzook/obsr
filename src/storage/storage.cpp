
#include "obsr_except.h"
#include "storage.h"


namespace obsr::storage {

storage::storage()
    : m_object_handles()
    , m_entry_handles()
    , m_root(empty_handle) {
    m_root = create_new_object(nullptr, "");
}

object storage::get_root() const {
    return m_root;
}

object storage::get_or_create_child(object object, const std::string_view& name) {
    auto data = m_object_handles[object];

    auto it = data->children.find(name);
    if (it == data->children.end()) {
        return create_new_object(data, name);
    }

    const auto child_handle = it->second;
    if (m_object_handles.has(child_handle)) {
        return child_handle;
    }

    // todo: better handling
    throw no_such_handle_exception(child_handle);
}

entry storage::get_or_create_entry(object object, const std::string_view& name) {
    auto data = m_object_handles[object];

    auto it = data->entries.find(name);
    if (it == data->entries.end()) {
        return create_new_object(data, name);
    }

    const auto entry_handle = it->second;
    if (m_entry_handles.has(entry_handle)) {
        return entry_handle;
    }

    // todo: better handling
    throw no_such_handle_exception(entry_handle);
}

void storage::get_entry_value(entry entry, value& value) {
    auto data = m_entry_handles[entry];
    value = data->value;
}

void storage::set_entry_value(entry entry, const value& value) {
    auto data = m_entry_handles[entry];

    const auto old_type = data->value.type;
    const auto new_type = value.type;
    if (old_type != value_type::empty && old_type != new_type) {
        throw entry_type_mismatch_exception(entry, old_type, new_type);
    }

    auto old_value = data->value;
    data->value = value;

    report_entry_value_change(data, old_value, value);
}

object storage::create_new_object(object_data* parent, const std::string_view& name) {
    auto handle = m_object_handles.allocate_new();

    auto data = m_object_handles[handle];
    data->handle = handle;
    data->name = name;

    if (parent != nullptr) {
        parent->children.emplace(name, handle);
    }

    report_new_object(data);

    return handle;
}

entry storage::create_new_child(object_data* parent, const std::string_view& name) {
    auto handle = m_entry_handles.allocate_new();

    auto data = m_entry_handles[handle];
    data->handle = handle;
    data->name = name;
    data->value.type = value_type::empty;

    parent->entries.emplace(name, handle);

    report_new_entry(data);

    return handle;
}

void storage::report_new_object(const object_data* object) {

}

void storage::report_new_entry(const entry_data* entry) {

}

void storage::report_entry_value_change(const entry_data* entry, const value& old_value, const value& new_value) {

}

}
