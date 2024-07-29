
#include "obsr_types.h"
#include "obsr_except.h"


namespace obsr {

invalid_handle_exception::invalid_handle_exception(handle handle)
    : m_handle(handle) {

}

no_such_handle_exception::no_such_handle_exception(handle handle)
    : m_handle(handle) {

}

entry_type_mismatch_exception::entry_type_mismatch_exception(entry entry, value_type actual_type, value_type new_type)
    : m_entry(entry)
    , m_actual_type(actual_type)
    , m_new_type(new_type) {

}

}
