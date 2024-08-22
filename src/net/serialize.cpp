
#include "io/serialize.h"

#include "serialize.h"

namespace obsr::net {

static constexpr size_t writer_buffer_size = 512;

message_parser::message_parser()
    : state_machine()
    , m_type(static_cast<message_type>(-1))
    , m_buffer()
{}

void message_parser::set_data(message_type type, const uint8_t* buffer, size_t size) {
    m_type = type;
    m_buffer.reset(buffer, size);
    reset();
}

bool message_parser::process_state(parse_state current_state, parse_data& data) {
    switch (current_state) {
        case parse_state::check_type: {
            return select_next_state(current_state);
        }
        case parse_state::read_id: {
            if (!io::read16(m_buffer, data.id)) {
                return error(error_read_data);
            }

            return select_next_state(current_state);
        }
        case parse_state::read_name: {
            size_t size = sizeof(data.name_buffer);
            if (!io::readraw(m_buffer, data.name_buffer, size)) {
                return error(error_read_data);
            }

            data.name = std::string(reinterpret_cast<char*>(data.name_buffer), size);

            return select_next_state(current_state);
        }
        case parse_state::read_type: {
            if (!io::read8(m_buffer, reinterpret_cast<uint8_t&>(data.type))) {
                return error(error_read_data);
            }

            return select_next_state(current_state);
        }
        case parse_state::read_value: {
            if (!io::read(m_buffer, data.type, data.value)) {
                return error(error_read_data);
            }

            data.value.type = data.type;

            return select_next_state(current_state);
        }
        case parse_state::read_time: {
            uint64_t value;
            if (!io::read64(m_buffer, value)) {
                return error(error_read_data);
            }

            data.time = std::chrono::milliseconds(value);
            return select_next_state(current_state);
        }
        case parse_state::read_start_time: {
            uint64_t value;
            if (!io::read64(m_buffer, value)) {
                return error(error_read_data);
            }

            data.start_time = std::chrono::milliseconds(value);
            return select_next_state(current_state);
        }
        case parse_state::read_end_time: {
            uint64_t value;
            if (!io::read64(m_buffer, value)) {
                return error(error_read_data);
            }

            data.end_time = std::chrono::milliseconds(value);
            return select_next_state(current_state);
        }
        default:
            return error(error_unknown_state);
    }
}

bool message_parser::select_next_state(parse_state current_state) {
    switch (current_state) {
        case parse_state::check_type: {
            switch (m_type) {
                case message_type::entry_create:
                case message_type::entry_update:
                case message_type::entry_delete:
                case message_type::entry_id_assign:
                    return move_to_state(parse_state::read_id);
                case message_type::handshake_ready:
                case message_type::handshake_finished:
                    return finished();
                case message_type::time_sync_request:
                    return move_to_state(parse_state::read_time);
                case message_type::time_sync_response:
                    return move_to_state(parse_state::read_start_time);
                default:
                    return error(error_unknown_type);
            }
        }
        case parse_state::read_id: {
            switch (m_type) {
                case message_type::entry_create:
                case message_type::entry_update:
                case message_type::entry_delete:
                    return move_to_state(parse_state::read_time);
                case message_type::entry_id_assign:
                    return move_to_state(parse_state::read_name);
                default:
                    return error(error_unknown_type);
            }
        }
        case parse_state::read_name: {
            switch (m_type) {
                case message_type::entry_create:
                    return move_to_state(parse_state::read_type);
                case message_type::entry_id_assign:
                    return finished();
                default:
                    return error(error_unknown_type);
            }
        }
        case parse_state::read_type: {
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
        case parse_state::read_time: {
            switch (m_type) {
                case message_type::entry_create:
                    return move_to_state(parse_state::read_name);
                case message_type::entry_update:
                    return move_to_state(parse_state::read_type);
                case message_type::entry_delete:
                case message_type::time_sync_request:
                    return finished();
                default:
                    return error(error_unknown_type);
            }
        }
        case parse_state::read_start_time: {
            switch (m_type) {
                case message_type::time_sync_response:
                    return move_to_state(parse_state::read_end_time);
                default:
                    return error(error_unknown_type);
            }
        }
        case parse_state::read_end_time: {
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

bool message_serializer::entry_id_assign(storage::entry_id id, std::string_view name) {
    if (!io::write16(m_buffer, id)) {
        return false;
    }

    if (!io::writeraw(m_buffer, reinterpret_cast<const uint8_t*>(name.data()), name.size())) {
        return false;
    }

    return true;
}

bool message_serializer::entry_created(std::chrono::milliseconds time, storage::entry_id id, std::string_view name, const value& value) {
    if (!io::write16(m_buffer, id)) {
        return false;
    }

    if (!io::write64(m_buffer, time.count())) {
        return false;
    }

    if (!io::writeraw(m_buffer, reinterpret_cast<const uint8_t*>(name.data()), name.size())) {
        return false;
    }

    if (!io::write8(m_buffer, static_cast<uint8_t>(value.type))) {
        return false;
    }

    if (!io::write(m_buffer, value.type, value)) {
        return false;
    }

    return true;
}

bool message_serializer::entry_updated(std::chrono::milliseconds time, storage::entry_id id, const value& value) {
    if (!io::write16(m_buffer, id)) {
        return false;
    }

    if (!io::write64(m_buffer, time.count())) {
        return false;
    }

    if (!io::write8(m_buffer, static_cast<uint8_t>(value.type))) {
        return false;
    }

    if (!io::write(m_buffer, value.type, value)) {
        return false;
    }

    return true;
}

bool message_serializer::entry_deleted(std::chrono::milliseconds time, storage::entry_id id) {
    if (!io::write16(m_buffer, id)) {
        return false;
    }

    if (!io::write64(m_buffer, time.count())) {
        return false;
    }

    return true;
}

bool message_serializer::time_sync_request(std::chrono::milliseconds start_time) {
    if (!io::write64(m_buffer, static_cast<uint64_t>(start_time.count()))) {
        return false;
    }

    return true;
}

bool message_serializer::time_sync_response(std::chrono::milliseconds start_time, std::chrono::milliseconds end_time) {
    if (!io::write64(m_buffer, static_cast<uint64_t>(start_time.count()))) {
        return false;
    }

    if (!io::write64(m_buffer, static_cast<uint64_t>(end_time.count()))) {
        return false;
    }

    return true;
}

message_queue::message_queue(destination* destination)
    : m_destination(destination)
    , m_serializer()
    , m_outgoing()
{}

void message_queue::enqueue(const out_message& message, uint8_t flags) {
    if ((flags & flag_immediate) != 0) {
        if (write_message(message)) {
            // success!
            return;
        } else {
            m_outgoing.push_front(message);
        }
    } else {
        m_outgoing.push_back(message);
    }
}

void message_queue::clear() {
    m_outgoing.clear();
}

void message_queue::process() {
    auto it = m_outgoing.begin();
    while (it != m_outgoing.end()) {
        const auto success = write_message(*it);
        if (success) {
            it = m_outgoing.erase(it);
        } else {
            break;
        }
    }
}

bool message_queue::write_message(const out_message& message) {
    switch (message.type) {
        case message_type::entry_create:
            return write_entry_created(message);
        case message_type::entry_update:
            return write_entry_updated(message);
        case message_type::entry_delete:
            return write_entry_deleted(message);
        case message_type::entry_id_assign:
            return write_entry_id_assigned(message);
        case message_type::time_sync_request:
            return write_time_sync_request(message);
        case message_type::time_sync_response:
            return write_time_sync_response(message);
        case message_type::handshake_ready:
        case message_type::handshake_finished:
            return write_basic(message);
        case message_type::no_type:
        default:
            return true;
    }
}

bool message_queue::write_entry_created(const out_message& message) {
    m_serializer.reset();

    if (!m_serializer.entry_created(message.update_time, message.id, message.name, message.value)) {
        return false;
    }

    if (!m_destination->write(
            static_cast<uint8_t>(message_type::entry_create),
            m_serializer.data(),
            m_serializer.size())) {
        return false;
    }

    return true;
}

bool message_queue::write_entry_updated(const out_message& message) {
    m_serializer.reset();

    if (!m_serializer.entry_updated(message.update_time, message.id, message.value)) {
        return false;
    }

    if (!m_destination->write(
            static_cast<uint8_t>(message_type::entry_update),
            m_serializer.data(),
            m_serializer.size())) {
        return false;
    }

    return true;
}

bool message_queue::write_entry_deleted(const out_message& message) {
    m_serializer.reset();

    if (!m_serializer.entry_deleted(message.update_time, message.id)) {
        return false;
    }

    if (!m_destination->write(
            static_cast<uint8_t>(message_type::entry_delete),
            m_serializer.data(),
            m_serializer.size())) {
        return false;
    }

    return true;
}

bool message_queue::write_entry_id_assigned(const out_message& message) {
    m_serializer.reset();

    if (!m_serializer.entry_id_assign(message.id, message.name)) {
        return false;
    }

    if (!m_destination->write(
            static_cast<uint8_t>(message_type::entry_id_assign),
            m_serializer.data(),
            m_serializer.size())) {
        return false;
    }

    return true;
}

bool message_queue::write_time_sync_request(const out_message& message) {
    m_serializer.reset();

    const auto time = m_destination->get_time_now();
    if (!m_serializer.time_sync_request(time)) {
        return false;
    }

    if (!m_destination->write(
            static_cast<uint8_t>(message_type::time_sync_request),
            m_serializer.data(),
            m_serializer.size())) {
        return false;
    }

    return true;
}

bool message_queue::write_time_sync_response(const out_message& message) {
    m_serializer.reset();

    const auto time = m_destination->get_time_now();
    if (!m_serializer.time_sync_response(message.time, time)) {
        return false;
    }

    if (!m_destination->write(
            static_cast<uint8_t>(message_type::time_sync_response),
            m_serializer.data(),
            m_serializer.size())) {
        return false;
    }

    return true;
}

bool message_queue::write_basic(const out_message& message) {
    if (!m_destination->write(
            static_cast<uint8_t>(message.type),
            nullptr,
            0)) {
        return false;
    }

    return true;
}

}
