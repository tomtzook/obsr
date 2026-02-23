
#include "obsr_internal.h"
#include "io/serialize.h"
#include "util/bits.h"

#include "serialize.h"

namespace obsr::net {

static constexpr size_t writer_buffer_size = 512;

void header_convert_net(message_header& header) {
    header.index = obsr::bits::net64(header.index);
    header.message_size = obsr::bits::net32(header.message_size);
}

void header_convert_host(message_header& header) {
    header.index = obsr::bits::host64(header.index);
    header.message_size = obsr::bits::host32(header.message_size);
}

out_message::out_message(const message_type type)
    : m_type(type)
    , m_id(0)
    , m_name()
    , m_value(value::make())
    , m_time(0)
    , m_send_time(0)
{}

message_type out_message::type() const {
    return m_type;
}

storage::entry_id out_message::id() const {
    assert(m_type == message_type::entry_create || m_type == message_type::entry_update || m_type == message_type::entry_delete || m_type == message_type::entry_id_assign);
    return m_id;
}

std::string_view out_message::name() const {
    assert(m_type == message_type::entry_create || m_type == message_type::entry_id_assign);
    return m_name;
}

const obsr::value& out_message::value() const {
    assert(m_type == message_type::entry_create || m_type == message_type::entry_update);
    return m_value;
}

std::chrono::milliseconds out_message::send_time() const {
    assert(m_type == message_type::entry_create || m_type == message_type::entry_update || m_type == message_type::entry_delete || m_type == message_type::entry_id_assign || m_type == message_type::time_sync_response || m_type == message_type::time_sync_request);
    return m_send_time;
}

std::chrono::milliseconds out_message::time_value() const {
    assert(m_type == message_type::time_sync_response);
    return m_time;
}

out_message out_message::empty() {
    return out_message();
}

out_message out_message::entry_create(
    const std::chrono::milliseconds send_time,
    const std::string_view name,
    obsr::value&& value) {
    out_message message(message_type::entry_create);
    message.m_send_time = send_time;
    message.m_name = name;
    message.m_value = std::move(value);

    return std::move(message);
}

out_message out_message::entry_update(
    const std::chrono::milliseconds send_time,
    const storage::entry_id id,
    obsr::value&& value) {
    out_message message(message_type::entry_update);
    message.m_send_time = send_time;
    message.m_id = id;
    message.m_value = std::move(value);

    return std::move(message);
}

out_message out_message::entry_deleted(
    const std::chrono::milliseconds send_time,
    const storage::entry_id id) {
    out_message message(message_type::entry_delete);
    message.m_send_time = send_time;
    message.m_id = id;

    return std::move(message);
}

out_message out_message::entry_id_assign(
    const storage::entry_id id,
    const std::string_view name) {
    out_message message(message_type::entry_id_assign);
    message.m_id = id;
    message.m_name = name;

    return std::move(message);
}

out_message out_message::handshake_ready() {
    return out_message(message_type::handshake_ready);
}

out_message out_message::handshake_finished() {
    return out_message(message_type::handshake_finished);
}

out_message out_message::time_sync_request(const std::chrono::milliseconds send_time) {
    out_message message(message_type::time_sync_request);
    message.m_send_time = send_time;

    return std::move(message);
}

out_message out_message::time_sync_response(
    const std::chrono::milliseconds send_time,
    const std::chrono::milliseconds time) {
    out_message message(message_type::time_sync_response);
    message.m_send_time = send_time;
    message.m_time = time;

    return std::move(message);
}

message_parser::message_parser()
    : state_machine(std::bind_front(&message_parser::process_state, this))
    , m_type(static_cast<message_type>(-1))
    , m_buffer()
    , m_deserializer(m_buffer)
{}

void message_parser::set_data(const message_type type, const uint8_t* buffer, const size_t size) {
    m_type = type;
    m_buffer.reset(buffer, size);
    reset();
}

bool message_parser::process_state(const parse_state current_state, parse_data& data) {
    switch (current_state) {
        case parse_state::check_type: {
            return select_next_state(current_state);
        }
        case parse_state::read_id: {
            const auto value_opt = m_deserializer.read16();
            if (!value_opt) {
                return error(error_read_data);
            }

            data.id = value_opt.value();
            return select_next_state(current_state);
        }
        case parse_state::read_name: {
            const auto value_opt = m_deserializer.read_str();
            if (!value_opt) {
                return error(error_read_data);
            }

            const auto value = value_opt.value();
            data.name = std::string(value);
            return select_next_state(current_state);
        }
        case parse_state::read_value_type: {
            const auto value_opt = m_deserializer.read8();
            if (!value_opt) {
                return error(error_read_data);
            }

            data.type = static_cast<value_type>(value_opt.value());
            return select_next_state(current_state);
        }
        case parse_state::read_value: {
            auto value_opt = m_deserializer.read_value(data.type);
            if (!value_opt) {
                return error(error_read_data);
            }

            data.value = std::move(value_opt.value());
            return select_next_state(current_state);
        }
        case parse_state::read_send_time: {
            const auto value_opt = m_deserializer.read64();
            if (!value_opt) {
                return error(error_read_data);
            }

            data.send_time = std::chrono::milliseconds(value_opt.value());
            return select_next_state(current_state);
        }
        case parse_state::read_time_value: {
            const auto value_opt = m_deserializer.read64();
            if (!value_opt) {
                return error(error_read_data);
            }

            data.time_value = std::chrono::milliseconds(value_opt.value());
            return select_next_state(current_state);
        }
        default:
            return error(error_unknown_state);
    }
}

bool message_parser::select_next_state(const parse_state current_state) {
    switch (current_state) {
        case parse_state::check_type: {
            switch (m_type) {
                case message_type::entry_create:
                case message_type::entry_update:
                case message_type::entry_delete:
                case message_type::time_sync_request:
                case message_type::time_sync_response:
                    return move_to_state(parse_state::read_send_time);
                case message_type::entry_id_assign:
                    return move_to_state(parse_state::read_id);
                case message_type::handshake_ready:
                case message_type::handshake_finished:
                    return finished();
                default:
                    return error(error_unknown_type);
            }
        }
        case parse_state::read_id: {
            switch (m_type) {
                case message_type::entry_id_assign:
                    return move_to_state(parse_state::read_name);
                case message_type::entry_update:
                    return move_to_state(parse_state::read_value_type);
                case message_type::entry_delete:
                    return finished();
                default:
                    return error(error_unknown_type);
            }
        }
        case parse_state::read_name: {
            switch (m_type) {
                case message_type::entry_create:
                    return move_to_state(parse_state::read_value_type);
                case message_type::entry_id_assign:
                    return finished();
                default:
                    return error(error_unknown_type);
            }
        }
        case parse_state::read_value_type: {
            switch (m_type) {
                case message_type::entry_create:
                case message_type::entry_update:
                    return move_to_state(parse_state::read_value);
                default:
                    return error(error_unknown_type);
            }
        }
        case parse_state::read_value: {
            switch (m_type) {
                case message_type::entry_create:
                case message_type::entry_update:
                    return finished();
                default:
                    return error(error_unknown_type);
            }
        }
        case parse_state::read_send_time: {
            switch (m_type) {
                case message_type::entry_create:
                    return move_to_state(parse_state::read_name);
                case message_type::entry_update:
                case message_type::entry_delete:
                    return move_to_state(parse_state::read_id);
                case message_type::time_sync_request:
                    return finished();
                case message_type::time_sync_response:
                    return move_to_state(parse_state::read_time_value);
                default:
                    return error(error_unknown_type);
            }
        }
        case parse_state::read_time_value: {
            switch (m_type) {
                case message_type::time_sync_response:
                    return finished();
                default:
                    return error(error_unknown_type);
            }
        }
        default:
            return error(error_unknown_state);
    }
}

message_serializer::message_serializer()
    : m_buffer(writer_buffer_size)
    , m_serializer(m_buffer)
{}

const uint8_t* message_serializer::data() const {
    return m_buffer.data();
}

size_t message_serializer::size() const {
    return m_buffer.pos();
}

void message_serializer::reset() {
    m_buffer.reset();
}

bool message_serializer::entry_id_assign(const storage::entry_id id, const std::string_view name) {
    if (!m_serializer.write16(id)) {
        return false;
    }

    if (!m_serializer.write_str(name)) {
        return false;
    }

    return true;
}

bool message_serializer::entry_created(const std::chrono::milliseconds send_time, const std::string_view name, const value& value) {
    if (!m_serializer.write64(send_time.count())) {
        return false;
    }

    if (!m_serializer.write_str(name)) {
        return false;
    }

    if (!m_serializer.write8(static_cast<uint8_t>(value.get_type()))) {
        return false;
    }

    if (!m_serializer.write_value(value)) {
        return false;
    }

    return true;
}

bool message_serializer::entry_updated(const std::chrono::milliseconds send_time, const storage::entry_id id, const value& value) {
    if (!m_serializer.write64(send_time.count())) {
        return false;
    }

    if (!m_serializer.write16(id)) {
        return false;
    }

    if (!m_serializer.write8(static_cast<uint8_t>(value.get_type()))) {
        return false;
    }

    if (!m_serializer.write_value(value)) {
        return false;
    }

    return true;
}

bool message_serializer::entry_deleted(const std::chrono::milliseconds send_time, const storage::entry_id id) {
    if (!m_serializer.write64(send_time.count())) {
        return false;
    }

    if (!m_serializer.write16(id)) {
        return false;
    }

    return true;
}

bool message_serializer::time_sync_request(const std::chrono::milliseconds send_time) {
    if (!m_serializer.write64(static_cast<uint64_t>(send_time.count()))) {
        return false;
    }

    return true;
}

bool message_serializer::time_sync_response(const std::chrono::milliseconds send_time, const std::chrono::milliseconds request_time) {
    if (!m_serializer.write64(static_cast<uint64_t>(send_time.count()))) {
        return false;
    }

    if (!m_serializer.write64(static_cast<uint64_t>(request_time.count()))) {
        return false;
    }

    return true;
}

message_queue::message_queue(destination&& destination)
    : m_destination(std::move(destination))
    , m_serializer()
    , m_outgoing()
{}

void message_queue::enqueue(out_message&& message, const uint64_t message_id,
    const uint8_t flags, const client_id destination, const client_id source) {
    const auto to_enqueue = data{destination, source, message_id, std::move(message)};
    if ((flags & flag_immediate) != 0) {
        if (write_message(std::move(to_enqueue))) {
            // success!
            return;
        }

        m_outgoing.push_front(std::move(to_enqueue));
    } else {
        m_outgoing.push_back(std::move(to_enqueue));
    }
}

void message_queue::clear() {
    m_outgoing.clear();
}

void message_queue::process() {
    auto it = m_outgoing.begin();
    while (it != m_outgoing.end()) {
        if (write_message(*it)) {
            it = m_outgoing.erase(it);
        } else {
            break;
        }
    }
}

bool message_queue::write_message(const data& data) {
    switch (data.message.type()) {
        case message_type::entry_create:
            return write_entry_created(data);
        case message_type::entry_update:
            return write_entry_updated(data);
        case message_type::entry_delete:
            return write_entry_deleted(data);
        case message_type::entry_id_assign:
            return write_entry_id_assigned(data);
        case message_type::time_sync_request:
            return write_time_sync_request(data);
        case message_type::time_sync_response:
            return write_time_sync_response(data);
        case message_type::handshake_ready:
        case message_type::handshake_finished:
            return write_basic(data);
        case message_type::no_type:
        default:
            return true;
    }
}

bool message_queue::write_entry_created(const data& data) {
    m_serializer.reset();

    if (!m_serializer.entry_created(data.message.send_time(),
                                    data.message.name(),
                                    data.message.value())) {
        return false;
    }

    if (!m_destination(
            static_cast<uint8_t>(message_type::entry_create),
            m_serializer.data(),
            m_serializer.size(),
            data.destination,
            data.source,
            data.message_id)) {
        return false;
    }

    return true;
}

bool message_queue::write_entry_updated(const data& data) {
    m_serializer.reset();

    if (!m_serializer.entry_updated(data.message.send_time(),
                                    data.message.id(),
                                    data.message.value())) {
        return false;
    }

    if (!m_destination(
            static_cast<uint8_t>(message_type::entry_update),
            m_serializer.data(),
            m_serializer.size(),
            data.destination,
            data.source,
            data.message_id)) {
        return false;
    }

    return true;
}

bool message_queue::write_entry_deleted(const data& data) {
    m_serializer.reset();

    if (!m_serializer.entry_deleted(data.message.send_time(),
                                    data.message.id())) {
        return false;
    }

    if (!m_destination(
            static_cast<uint8_t>(message_type::entry_delete),
            m_serializer.data(),
            m_serializer.size(),
            data.destination,
            data.source,
            data.message_id)) {
        return false;
    }

    return true;
}

bool message_queue::write_entry_id_assigned(const data& data) {
    m_serializer.reset();

    if (!m_serializer.entry_id_assign(data.message.id(),
                                      data.message.name())) {
        return false;
    }

    if (!m_destination(
            static_cast<uint8_t>(message_type::entry_id_assign),
            m_serializer.data(),
            m_serializer.size(),
            data.destination,
            data.source,
            data.message_id)) {
        return false;
    }

    return true;
}

bool message_queue::write_time_sync_request(const data& data) {
    m_serializer.reset();

    if (!m_serializer.time_sync_request(data.message.send_time())) {
        return false;
    }

    if (!m_destination(
            static_cast<uint8_t>(message_type::time_sync_request),
            m_serializer.data(),
            m_serializer.size(),
            data.destination,
            data.source,
            data.message_id)) {
        return false;
    }

    return true;
}

bool message_queue::write_time_sync_response(const data& data) {
    m_serializer.reset();

    if (!m_serializer.time_sync_response(data.message.send_time(), data.message.time_value())) {
        return false;
    }

    if (!m_destination(
            static_cast<uint8_t>(message_type::time_sync_response),
            m_serializer.data(),
            m_serializer.size(),
            data.destination,
            data.source,
            data.message_id)) {
        return false;
    }

    return true;
}

bool message_queue::write_basic(const data& data) {
    if (!m_destination(
            static_cast<uint8_t>(data.message.type()),
            nullptr,
            0,
            data.destination,
            data.source,
            data.message_id)) {
        return false;
    }

    return true;
}

}
