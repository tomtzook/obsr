#pragma once

#include <looper.h>

#include "io/buffer.h"
#include "util/state.h"
#include "net/serialize.h"

namespace obsr::net {

struct connection_info {
    std::string ip;
    uint16_t port;
};

struct read_data {
    static constexpr size_t message_buffer_size = 1024;
    message_header header;
    uint8_t message_buffer[message_buffer_size];
};

enum class read_state {
    header,
    message
};

enum read_error {
    read_unsupported_size = 1,
    read_unknown_state = 2,
    read_failed = 3
};

class reader final : public state_machine<read_state, read_state::header, read_data> {
public:
    explicit reader(size_t buffer_size);

    bool update(std::span<const uint8_t> buffer);

private:
    bool process_state(read_state current_state, read_data& data);

    obsr::io::circular_buffer m_read_buffer;
};

}
