
#include "internal_except.h"
#include "util/general.h"

#include "io.h"

namespace obsr::net {

reader::reader(const size_t buffer_size)
    : state_machine(std::bind_front(&reader::process_state, this))
    , m_read_buffer(buffer_size) {
}

bool reader::update(const std::span<const uint8_t> buffer) {
    return m_read_buffer.write(buffer.data(), buffer.size_bytes());
}

bool reader::process_state(const read_state current_state, read_data& data) {
    switch (current_state) {
        case read_state::header: {
            auto& header = data.header;
            do {
                auto success = m_read_buffer.find_and_seek_read(message_header::message_magic);
                if (!success) {
                    return try_later();
                }

                success = m_read_buffer.read(header);
                if (!success) {
                    return try_later();
                }

                header_convert_host(header);
            } while (header.magic != message_header::message_magic ||
                     header.version != message_header::current_version);

            return move_to_state(read_state::message);
        }
        case read_state::message: {
            const auto& header = data.header;
            const auto buffer = data.message_buffer;
            if (header.message_size > read_data::message_buffer_size) {
                // we can skip forward by the size, but regardless we will handle it fine because
                // we jump to the magic
                m_read_buffer.seek_read(header.message_size);
                return error(read_error::read_unsupported_size);
            }

            if (!m_read_buffer.can_read(header.message_size)) {
                return try_later();
            }

            if (!m_read_buffer.read(buffer, header.message_size)) {
                return error(read_error::read_failed);
            }

            return finished();
        }
        default:
            return error(read_error::read_unknown_state);
    }
}

}
