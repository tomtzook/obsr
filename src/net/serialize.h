#pragma once

#include <cstddef>

#include "io/buffer.h"
#include "io/serialize.h"
#include "util/state.h"
#include "storage/storage.h"

namespace obsr::net {

#pragma pack(push, 1)
struct message_header {
    static constexpr uint8_t message_magic = 0x29;
    static constexpr uint8_t current_version = 0x1;

    uint8_t magic;
    uint8_t version;
    uint32_t index;
    uint8_t type;
    uint32_t message_size;
};
#pragma pack(pop)

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
    read_value_type,
    read_value,
    read_send_time,
    read_time_value
};

enum parse_error {
    error_unknown_type,
    error_read_data,
    error_unknown_state
};

struct parse_data {
    std::chrono::milliseconds send_time;

    storage::entry_id id;
    std::string name;
    value_type type;
    obsr::value value = obsr::value::make();
    std::chrono::milliseconds time_value;
};

struct out_message {
public:
    explicit out_message(message_type type = message_type::no_type)
        : m_type(type)
        , m_id(0)
        , m_name()
        , m_value(value::make())
        , m_time(0)
        , m_send_time(0)
    {}

    inline message_type type() const {
        return m_type;
    }

    inline storage::entry_id id() const {
        assert(m_type == message_type::entry_create || m_type == message_type::entry_update || m_type == message_type::entry_delete || m_type == message_type::entry_id_assign);
        return m_id;
    }

    inline std::string_view name() const {
        assert(m_type == message_type::entry_create || m_type == message_type::entry_id_assign);
        return m_name;
    }

    inline const obsr::value& value() const {
        assert(m_type == message_type::entry_create || m_type == message_type::entry_update);
        return m_value;
    }

    inline std::chrono::milliseconds send_time() const {
        assert(m_type == message_type::entry_create || m_type == message_type::entry_update || m_type == message_type::entry_delete || m_type == message_type::entry_id_assign || m_type == message_type::time_sync_response || m_type == message_type::time_sync_request);
        return m_send_time;
    }

    inline std::chrono::milliseconds time_value() const {
        assert(m_type == message_type::time_sync_response);
        return m_time;
    }

    static inline out_message empty() {
        return out_message();
    }

    static inline out_message entry_create(std::chrono::milliseconds send_time, std::string_view name, obsr::value&& value) {
        out_message message(message_type::entry_create);
        message.m_send_time = send_time;
        message.m_name = name;
        message.m_value = std::move(value);

        return std::move(message);
    }

    static inline out_message entry_update(std::chrono::milliseconds send_time, storage::entry_id id, obsr::value&& value) {
        out_message message(message_type::entry_update);
        message.m_send_time = send_time;
        message.m_id = id;
        message.m_value = std::move(value);

        return std::move(message);
    }

    static inline out_message entry_deleted(std::chrono::milliseconds send_time, storage::entry_id id) {
        out_message message(message_type::entry_delete);
        message.m_send_time = send_time;
        message.m_id = id;

        return std::move(message);
    }
    static inline out_message entry_id_assign(storage::entry_id id, std::string_view name) {
        out_message message(message_type::entry_id_assign);
        message.m_id = id;
        message.m_name = name;

        return std::move(message);
    }

    static inline out_message handshake_ready() {
        return out_message(message_type::handshake_ready);
    }

    static inline out_message handshake_finished() {
        return out_message(message_type::handshake_finished);
    }

    static inline out_message time_sync_request(std::chrono::milliseconds send_time) {
        out_message message(message_type::time_sync_request);
        message.m_send_time = send_time;

        return std::move(message);
    }

    static inline out_message time_sync_response(std::chrono::milliseconds send_time, std::chrono::milliseconds time) {
        out_message message(message_type::time_sync_response);
        message.m_send_time = send_time;
        message.m_time = time;

        return std::move(message);
    }

private:
    message_type m_type;

    storage::entry_id m_id;
    std::string m_name;
    obsr::value m_value;
    std::chrono::milliseconds m_time;
    std::chrono::milliseconds m_send_time;
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
    io::deserializer m_deserializer;
};

class message_serializer {
public:
    message_serializer();

    const uint8_t* data() const;
    size_t size() const;

    void reset();

    bool entry_id_assign(storage::entry_id id, std::string_view name);
    bool entry_created(std::chrono::milliseconds send_time, std::string_view name, const value& value);
    bool entry_updated(std::chrono::milliseconds send_time, storage::entry_id id, const value& value);
    bool entry_deleted(std::chrono::milliseconds send_time, storage::entry_id id);
    bool time_sync_request(std::chrono::milliseconds send_time);
    bool time_sync_response(std::chrono::milliseconds send_time, std::chrono::milliseconds request_time);
private:
    io::linear_buffer m_buffer;
    io::serializer m_serializer;
};

class message_queue {
public:
    using destination = std::function<bool(uint8_t, const uint8_t*, size_t)>;
    enum {
        flag_immediate = 1 << 0
    };

    message_queue();

    void attach(destination destination);

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

    destination m_destination;

    message_serializer m_serializer;
    std::deque<out_message> m_outgoing;
};

}
