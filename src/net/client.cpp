
#include <utility>

#include "debug.h"
#include "internal_except.h"
#include "client.h"

namespace obsr::net {

#define LOG_MODULE "client"

client::client(std::shared_ptr<io::nio_runner> nio_runner)
    : m_state(state::idle)
    , m_mutex()
    , m_io(std::move(nio_runner), this)
    , m_conn_info()
    , m_parser()
    , m_storage()
{}

void client::attach_to_storage(storage_link* storage_link) {
    m_storage.set_storage(storage_link);
    storage_link->attach_net_link(&m_storage);
}

void client::start(connection_info info) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::idle) {
        throw illegal_state_exception();
    }

    m_conn_info = std::move(info);
    m_state = state::opening;
}

void client::stop() {
    std::unique_lock lock(m_mutex);

    if (m_state == state::idle) {
        throw illegal_state_exception();
    }
}

void client::process() {
    std::unique_lock lock(m_mutex);

    switch (m_state) {
        case state::opening: {
            // todo: we may want a delay before trying again
            //  our caller is delayed, but we may want more
            if (open_socket_and_start_connection()) {
                // successful
                m_state = state::connecting;
            }

            break;
        }
        case state::in_handshake:
            break;
        case state::in_use:
            break;

        case state::connecting:
        case state::idle:
        default:
            break;
    }
}

void client::on_new_message(const message_header& header, const uint8_t* buffer, size_t size) {
    std::unique_lock lock(m_mutex);

    auto type = static_cast<message_type>(header.type);
    m_parser.set_data(type, buffer, size);
    m_parser.process();

    if (m_parser.is_errored()) {
        TRACE_DEBUG(LOG_MODULE, "failed to parse incoming data, parser error=%d", m_parser.error_code());
        return;
    } else if (!m_parser.is_finished()) {
        TRACE_DEBUG(LOG_MODULE, "failed to parse incoming data, parser did not finish");
        return;
    }

    auto parse_data = m_parser.data();
    switch (type) {
        case message_type::entry_create:
            m_storage.on_entry_created(parse_data.id, parse_data.name, parse_data.value);
            break;
        case message_type::entry_update:
            m_storage.on_entry_updated(parse_data.id, parse_data.value);
            break;
        case message_type::entry_delete:
            m_storage.on_entry_deleted(parse_data.id);
            break;
    }
}

void client::on_connected() {
    std::unique_lock lock(m_mutex);

    m_state = state::in_handshake;
}

void client::on_close() {
    std::unique_lock lock(m_mutex);

    // todo: clean up resources
    m_state = state::opening;
}

bool client::open_socket_and_start_connection() {
    try {
        auto socket = std::make_shared<obsr::os::socket>();
        socket->setoption<os::sockopt_reuseport>(true);

        m_io.start(socket);
        m_io.connect(m_conn_info);

        return true;
    } catch (...) {
        if (!m_io.is_stopped()) {
            m_io.stop();
        }

        return false;
    }
}

}
