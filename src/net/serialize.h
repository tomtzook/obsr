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
    handshake_finished = 5,
    handshake_ready = 6,
    time_sync_request = 7,
    time_sync_response = 8,
};

enum class parse_state {
    check_type,
    read_id,
    read_name,
    read_type,
    read_value,
    read_time,
    read_time_value
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
    obsr::value_raw value;

    uint8_t name_buffer[1024];

    std::chrono::milliseconds time;
    std::chrono::milliseconds time_value;
};

struct out_message {
    message_type type;

    storage::entry_id id;
    std::string name;
    obsr::value_raw value;
    std::chrono::milliseconds time;
    std::chrono::milliseconds update_time;

    static inline out_message empty() {
        out_message message{.type = message_type::no_type};
        return message;
    }
    static inline out_message entry_create(std::chrono::milliseconds update_time, storage::entry_id id, std::string_view name, const obsr::value_raw& value) {
        out_message message{.type = message_type::entry_create};
        message.id = id;
        message.name = name;
        message.value = value;
        message.update_time = update_time;

        return message;
    }
    static inline out_message entry_update(std::chrono::milliseconds update_time, storage::entry_id id, const obsr::value_raw& value) {
        out_message message{.type = message_type::entry_update};
        message.id = id;
        message.value = value;
        message.update_time = update_time;

        return message;
    }
    static inline out_message entry_deleted(std::chrono::milliseconds update_time, storage::entry_id id) {
        out_message message{.type = message_type::entry_delete};
        message.id = id;
        message.update_time = update_time;

        return message;
    }
    static inline out_message entry_id_assign(storage::entry_id id, std::string_view name) {
        out_message message{.type = message_type::entry_id_assign};
        message.id = id;
        message.name = name;

        return message;
    }
    static inline out_message handshake_ready() {
        out_message message{.type = message_type::handshake_ready};
        return message;
    }
    static inline out_message handshake_finished() {
        out_message message{.type = message_type::handshake_finished};
        return message;
    }
    static inline out_message time_sync_request(std::chrono::milliseconds update_time) {
        out_message message{.type = message_type::time_sync_request};
        message.update_time = update_time;
        return message;
    }
    static inline out_message time_sync_response(std::chrono::milliseconds update_time, std::chrono::milliseconds time) {
        out_message message{.type = message_type::time_sync_response};
        message.time = time;
        message.update_time = update_time;
        return message;
    }
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

class message_serializer {
public:
    message_serializer();

    const uint8_t* data() const;
    size_t size() const;

    void reset();

    bool entry_id_assign(storage::entry_id id, std::string_view name);
    bool entry_created(std::chrono::milliseconds time, storage::entry_id id, std::string_view name, const value_raw& value);
    bool entry_updated(std::chrono::milliseconds time, storage::entry_id id, const value_raw& value);
    bool entry_deleted(std::chrono::milliseconds time, storage::entry_id id);
    bool time_sync_request(std::chrono::milliseconds start_time);
    bool time_sync_response(std::chrono::milliseconds client_time, std::chrono::milliseconds server_time);
private:
    io::linear_buffer m_buffer;
};

class message_queue {
public:
    class destination {
    public:
        virtual bool write(uint8_t type, const uint8_t* buffer, size_t size) = 0;
    };
    enum {
        flag_immediate = 1 << 0
    };

    explicit message_queue(destination* destination);

    // todo: optimize by only writing the latest message for an entry (not including publish)
    // todo: try and switch to sending only the latest state instead of queueing every change
    //      only relevant if we can't keep up with changes
    // todo: entry create should be a client only message without id to report new entry needing id assignment
    void enqueue(const out_message& message, uint8_t flags = 0);
    void clear();

    void process();

private:
    bool write_message(const out_message& message);
    bool write_entry_created(const out_message& message);
    bool write_entry_updated(const out_message& message);
    bool write_entry_deleted(const out_message& message);
    bool write_entry_id_assigned(const out_message& message);
    bool write_time_sync_request(const out_message& message);
    bool write_time_sync_response(const out_message& message);
    bool write_basic(const out_message& message);

    destination* m_destination;

    message_serializer m_serializer;
    std::deque<out_message> m_outgoing;
};

}
