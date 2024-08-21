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
    read_start_time,
    read_end_time
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
    obsr::value value;

    uint8_t name_buffer[1024];

    std::chrono::milliseconds time;
    std::chrono::milliseconds start_time;
    std::chrono::milliseconds end_time;
};

struct out_message {
    message_type type;

    storage::entry_id id;
    std::string name;
    obsr::value value;
    std::chrono::milliseconds time;
    std::chrono::milliseconds update_time;
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
    bool entry_created(std::chrono::milliseconds time, storage::entry_id id, std::string_view name, const value& value);
    bool entry_updated(std::chrono::milliseconds time, storage::entry_id id, const value& value);
    bool entry_deleted(std::chrono::milliseconds time, storage::entry_id id);
    bool time_sync_request(std::chrono::milliseconds start_time);
    bool time_sync_response(std::chrono::milliseconds start_time, std::chrono::milliseconds end_time);
private:
    io::linear_buffer m_buffer;
};

class message_queue {
public:
    class destination {
    public:
        virtual std::chrono::milliseconds get_time_now() = 0;

        virtual bool write(uint8_t type, const uint8_t* buffer, size_t size) = 0;
    };

    explicit message_queue(destination* destination);

    // todo: optimize by only writing the latest message for an entry (not including publish)
    // todo: entry create should be a client only message without id to report new entry needing id assignment
    void enqueue(const out_message& message);
    void clear();

    // todo: make functions to create the out message instead of enqueue
    inline void enqueue_entry_create(storage::entry_id id, std::string_view name, const obsr::value& value) {
        out_message message{.type = message_type::entry_create};
        message.id = id;
        message.name = name;
        message.value = value;
        message.update_time = m_destination->get_time_now();
        enqueue(message);
    }
    inline void enqueue_entry_update(storage::entry_id id, const obsr::value& value) {
        out_message message{.type = message_type::entry_update};
        message.id = id;
        message.value = value;
        message.update_time = m_destination->get_time_now();
        enqueue(message);
    }
    inline void enqueue_entry_deleted(storage::entry_id id) {
        out_message message{.type = message_type::entry_delete};
        message.id = id;
        message.update_time = m_destination->get_time_now();
        enqueue(message);
    }
    inline void enqueue_entry_id_assign(storage::entry_id id, std::string_view name) {
        out_message message{.type = message_type::entry_id_assign};
        message.id = id;
        message.name = name;
        enqueue(message);
    }
    inline void enqueue_handshake_ready() {
        out_message message{.type = message_type::handshake_ready};
        enqueue(message);
    }
    inline void enqueue_handshake_finished() {
        out_message message{.type = message_type::handshake_finished};
        enqueue(message);
    }
    inline void enqueue_time_sync_request() {
        out_message message{.type = message_type::time_sync_request};
        enqueue(message);
    }
    inline void enqueue_time_sync_response(std::chrono::milliseconds time) {
        out_message message{.type = message_type::time_sync_response};
        message.time = time;
        enqueue(message);
    }

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
