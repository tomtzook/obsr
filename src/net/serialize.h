#pragma once

#include <cstddef>

#include "io/buffer.h"
#include "util/state.h"
#include "storage/storage.h"

namespace obsr::net {

enum class message_type {
    no_type,
    entry_create = 1,
    entry_update = 2,
    entry_delete = 3,
    entry_id_assign = 4,
    handshake_finished = 5
};

enum class parse_state {
    check_type,
    read_id,
    read_name,
    read_type,
    read_value
};

enum parse_error {
    error_unknown_type,
    error_read_data,
    error_unknown_state
};

struct parse_data {
    storage::entry_id id;
    std::string name;
    value_type type;
    value_t value;

    uint8_t name_buffer[1024];
};

class message_parser : public state_machine<parse_state, parse_state::check_type, parse_data> {
public:
    message_parser();

    void set_data(message_type type, const uint8_t* buffer, size_t size);

protected:
    bool process_state(parse_state current_state, parse_data& data) override;

private:
    bool select_next_state(parse_state current_state);

    message_type m_type;
    io::readonly_buffer m_buffer;
};

class message_writer {
public:
    message_writer();

    const uint8_t* data() const;
    size_t size() const;

    void reset();

    bool entry_id_assign(storage::entry_id id, std::string_view name);
    bool entry_created(storage::entry_id id, std::string_view name, const value_t& value);
    bool entry_updated(storage::entry_id id, const value_t& value);
    bool entry_deleted(storage::entry_id id);
private:
    io::linear_buffer m_buffer;
};

}
