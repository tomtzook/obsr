
#include "io/serialize.h"
#include "os/io.h"
#include "internal_except.h"
#include "debug.h"

#include "client.h"

namespace obsr::net {

// todo: organize better by dividing into smaller more managed units.
// todo: two basic read states: read header, read message

// todo: add logging


/*client_reader_old::client_reader_old()
    : m_read_buffer(1024)
    , m_read_state(read_state::start)
    , m_error_state(error_state::no_error)
    , m_state() {

}

void client_reader_old::reset() {
    m_read_buffer.reset();
    m_read_state = read_state::start;
}

void client_reader_old::update(obsr::os::readable* readable) {
    m_read_buffer.read_from(readable);
}

std::optional<client_reader_old::result> client_reader_old::process() {
    while (process_once());

    if (m_read_state == read_state::error) {
        return {{m_error_state, {}}};
    } else if (m_read_state == read_state::end) {
        m_read_state = read_state::start;
        return {{error_state::no_error, m_state}};
    }

    return {};
}

bool client_reader_old::process_once() {
    switch (m_read_state) {
        case read_state::start: {
            auto& header = m_state.m_header;
            do {
                auto success = m_read_buffer.read(header);
                if (!success) {
                    return false;
                }
            } while (header.magic != message_header::message_magic ||
                    header.version != message_header::current_version);

            return redirect_by_type();
        }
        case read_state::error:
            return false;
        case read_state::entryassign_1namesize: {
            if (!obsr::io::read64(m_read_buffer, reinterpret_cast<uint64_t&>(m_state.m_name_size))) {
                return false;
            }

            m_read_state = read_state::entryassign_2name;
            return true;
        }
        case read_state::entryassign_2name: {
            uint8_t buffer[1024]; // todo: dynamic buffer
            if (!m_read_buffer.read(buffer, m_state.m_name_size)) {
                return false;
            }

            m_read_state = read_state::entryassign_3type;
            return true;
        }
        case read_state::entryassign_3type: {
            if (!obsr::io::read8(m_read_buffer, reinterpret_cast<uint8_t &>(m_state.m_type))) {
                return false;
            }

            m_read_state = read_state::entryassign_4value;
            return true;
        }
        case read_state::entryassign_4value: {
            if (!obsr::io::read(m_read_buffer, m_state.m_type, m_state.m_value)) {
                return false;
            }

            m_state.m_value.type = m_state.m_type;
            m_read_state = read_state::end;
            return false;
        }
        default:
            on_error(error_state::unknown_state);
            return false;
    }
}

bool client_reader_old::redirect_by_type() {
    switch (m_state.m_header.type) {
        case message_type::handshake:
            back_to_start();
            break;
        case message_type::entry_assign:
            m_read_state = read_state::entryassign_1namesize;
            break;
        case message_type::entry_update:
            m_read_state = read_state::entryupdate_1namesize;
            break;
        case message_type::entry_delete:
            m_read_state = read_state::entrydelete_1namesize;
            break;
        default:
            on_error(error_state::unknown_message_type);
            return false;
    }

    return true;
}*/

#define LOG_MODULE "netclient"

client_reader::client_reader()
    : m_read_buffer(1024)
    , m_read_state(read_state::header)
    , m_error_state(error_state::no_error)
    , m_state() {
}

bool client_reader::has_full_result() const {
    return m_read_state == read_state::end;
}

bool client_reader::has_error() const {
    // todo: do something with error type
    return m_read_state == read_state::error;
}

const client_reader::state& client_reader::current_state() const {
    return m_state;
}

void client_reader::reset() {
    m_read_buffer.reset();
    m_read_state = read_state::header;
}

void client_reader::update(obsr::os::readable* readable) {
    m_read_buffer.read_from(readable);
}

void client_reader::process() {
    // reset trackers
    switch (m_read_state) {
        case read_state::error:
            m_error_state = error_state::no_error;
            m_read_state = read_state::start;
            break;
        case read_state::end:
            m_read_state = read_state::start;
            break;
        case read_state::start:
            m_error_state = error_state::no_error;
        default:
            break;
    }

    while (process_once());
}

bool client_reader::process_once() {
    switch (m_read_state) {
        case read_state::header: {
            auto& header = m_state.header;
            do {
                // todo: optimization for locating magic
                auto success = m_read_buffer.read(header);
                if (!success) {
                    return try_later();
                }
            } while (header.magic != message_header::message_magic ||
                     header.version != message_header::current_version);

            return move_to_state(read_state::message);
        }
        case read_state::message: {
            const auto& header = m_state.header;
            auto buffer = m_state.message_buffer;
            if (header.message_size > state::message_buffer_size) {
                // we can skip forward by the size, but regardless we will handle it fine because
                // we jump to the magic
                m_read_buffer.seek_read(header.message_size);
                return on_error(error_state::unsupported_size);
            }

            if (!m_read_buffer.can_read(header.message_size)) {
                return try_later();
            }

            // todo: eliminate this middleman
            if (!m_read_buffer.read(buffer, header.message_size)) {
                return on_error(error_state::read_failed);
            }

            return finished();
        }
        case read_state::end:
        case read_state::error:
            return false;
        default:
            return on_error(error_state::unknown_state);
    }
}

client_io::client_io(std::shared_ptr<obsr::io::nio_runner> nio_runner, listener* listener)
    : m_nio_runner(std::move(nio_runner))
    , m_resource_handle(empty_handle)
    , m_socket()
    , m_reader()
    , m_write_buffer(1024)
    , m_connecting(false)
    , m_closed(true)
    , m_listener(listener) {

}

client_io::~client_io() {
    stop();
}

bool client_io::is_closed() {
    std::unique_lock lock(m_mutex);

    return m_closed;
}

void client_io::start(std::shared_ptr<obsr::os::socket> socket) {
    std::unique_lock lock(m_mutex);

    m_reader.reset();
    m_write_buffer.reset();
    m_resource_handle = empty_handle;
    m_connecting = false;

    try {
        m_socket = std::move(socket);
        m_socket->configure_blocking(false);
        m_socket->setoption<obsr::os::sockopt_reuseport>(true);

        m_closed = false;
        m_resource_handle = m_nio_runner->add(m_socket,
                                              obsr::os::selector::poll_in,
                                              [this](obsr::os::resource& res, uint32_t flags)->void { on_ready_resource(flags); });
    } catch (...) {
        TRACE_DEBUG(LOG_MODULE, "start failed");
        stop();
    }
}

void client_io::stop() {
    std::unique_lock lock(m_mutex);

    TRACE_DEBUG(LOG_MODULE, "stop");
    try {
        if (m_resource_handle != empty_handle) {
            m_nio_runner->remove(m_resource_handle);
        }

        m_socket->close();
        m_socket.reset();

        if (m_listener != nullptr) {
            TRACE_DEBUG(LOG_MODULE, "stop listener called");
            m_listener->on_close();
        }
    } catch (...) {

    }

    m_closed = true;
}

void client_io::connect(connection_info info) {
    std::unique_lock lock(m_mutex);

    m_connecting = true;
    m_socket->connect(info.ip, info.port);

    m_nio_runner->add_flags(m_resource_handle, obsr::os::selector::poll_out);
}

bool client_io::process() {
    std::unique_lock lock(m_mutex);

    m_reader.process();

    if (m_reader.has_error()) {
        // todo: handle
        TRACE_DEBUG(LOG_MODULE, "read process error");
    } else if (m_reader.has_full_result()) {
        TRACE_DEBUG(LOG_MODULE, "new message processed");
        auto& state = m_reader.current_state();
        if (m_listener != nullptr) {
            m_listener->on_new_message(state.header, state.message_buffer, state.header.message_size);
        }
    }

    return !m_closed;
}

bool client_io::write(uint8_t type, uint8_t* buffer, size_t size) {
    message_header header {
            message_header::message_magic,
            message_header::current_version,
            0, // TODO: IMPLEMENT
            type,
            static_cast<uint32_t>(size)
    };

    if (!m_write_buffer.write(reinterpret_cast<uint8_t*>(&header), sizeof(header))) {
        TRACE_DEBUG(LOG_MODULE, "write failed to buffer at start");
        return false;
    }

    if (buffer != nullptr && size > 0) {
        if (!m_write_buffer.write(buffer, size)) {
            // this means we have probably sent a message with an header but no data. this will seriously
            // break down communication. as such, we will terminate connection here.
            // todo: add ability for remote to deal with this safely.
            TRACE_DEBUG(LOG_MODULE, "write attempt failed halfway, stopping");
            stop();
            return false;
        }
    }

    m_nio_runner->add_flags(m_resource_handle, obsr::os::selector::poll_out);

    return true;
}

void client_io::on_ready_resource(uint32_t flags) {
    std::unique_lock lock(m_mutex);

    if ((flags & obsr::os::selector::poll_in) != 0) {
        on_read_ready();
    }

    if ((flags & obsr::os::selector::poll_out) != 0) {
        on_write_ready();
    }
}

void client_io::on_read_ready() {
    TRACE_DEBUG(LOG_MODULE, "on read update");
    try {
        m_reader.update(m_socket.get());
    } catch (const eof_exception&) {
        // socket was closed
        TRACE_DEBUG(LOG_MODULE, "read eof");
        stop();
    } catch (...) {
        // any other error
        TRACE_DEBUG(LOG_MODULE, "read error");
        stop();
    }
}

void client_io::on_write_ready() {
    TRACE_DEBUG(LOG_MODULE, "on write update");
    if (m_connecting) {
        TRACE_DEBUG(LOG_MODULE, "connect finished");
        m_connecting = false;

        if (m_listener != nullptr) {
            m_listener->on_connected();
        }
    } else {
        try {
            TRACE_DEBUG(LOG_MODULE, "writing to socket");
            if (!m_write_buffer.write_into(m_socket.get())) {
                TRACE_DEBUG(LOG_MODULE, "nothing more to write");
                // nothing more to write
                m_nio_runner->remove_flags(m_resource_handle, obsr::os::selector::poll_out);
            }
        } catch (...) {
            TRACE_DEBUG(LOG_MODULE, "write error");
            stop();
        }
    }
}

/*running_client::running_client(std::shared_ptr<nio_runner> nio_runner)
    : m_nio_runner(std::move(nio_runner))
    , m_io(m_nio_runner)
    , m_info()
    , m_mutex()
    , m_current_state(state::idle) {
}

void running_client::process() {
    std::unique_lock lock(m_mutex);
    while (process_once());
}

bool running_client::process_once() {
    switch (m_current_state) {
        case state::opening:
            if (on_open_socket()) {
                m_current_state = state::connecting;
            }
            return false;
        case state::connected:
            break;
        case state::waiting_for_handshake:
            break;
        case state::waiting_for_go:
            break;
        case state::sent_initial_handshake:
            break;

        case state::idle:
        case state::connecting:
        default:
            return false;
    }
}

bool running_client::on_open_socket(bool connect) {
    auto socket = std::make_shared<obsr::os::socket>();

    try {
        m_io.start(socket);
        if (connect) {
            m_io.connect(m_info);
        }

        return true;
    } catch (...) {
        m_io.stop();
        return false;
    }
}

client::client(std::shared_ptr<nio_runner>& nio_runner)
    : m_nio_runner(std::move(nio_runner))
    , m_socket()
    , m_resource_handle(empty_handle)
    , m_reader()
    , m_write_buffer(1024)
    , m_mutex()
    , m_current_state(state::idle) {
}

client::~client() {

}

void client::start(connection_info info) {
    std::unique_lock lock(m_mutex);
    if (m_current_state != state::idle) {
        return;
    }

    m_reader.reset();
    m_write_buffer.reset();

    m_info = std::move(info);
    m_current_state = state::opening;
}

void client::start(std::shared_ptr<obsr::os::socket>& socket) {
    std::unique_lock lock(m_mutex);
    if (m_current_state != state::idle) {
        return;
    }

    m_reader.reset();
    m_write_buffer.reset();

    m_socket = socket;
    m_resource_handle = m_nio_runner->add(m_socket,
                                          obsr::os::selector::poll_in | obsr::os::selector::poll_out,
                                          [this](uint32_t flags)->void { on_ready_resource(flags); });

    write_handshake();
    m_current_state = state::sent_initial_handshake;
}

void client::process() {
    while (process_once());
}

void client::write_handshake() {
    write(message_type::handshake, nullptr, 0);
}

void client::write(message_type type, uint8_t* buffer, size_t size) {
    if (buffer == nullptr) {
        size = 0;
    } else if (size == 0) {
        buffer = nullptr;
    }

    message_header header {
        message_header::message_magic,
        message_header::current_version,
        0, // TODO: IMPLEMENT
        type,
        static_cast<uint32_t>(size)
    };

    io::writeraw(m_write_buffer, reinterpret_cast<uint8_t*>(&header), sizeof(header));
    if (buffer != nullptr) {
        io::writeraw(m_write_buffer, buffer, size);
    }
}

bool client::process_once() {
    std::unique_lock lock(m_mutex);



    switch (m_current_state) {
        case state::opening:
            if (on_open_socket()) {
                m_current_state = state::connecting;
            }
            return false;
        case state::connected:
            break;
        case state::waiting_for_handshake:
            break;
        case state::waiting_for_go:
            break;
        case state::sent_initial_handshake:
            break;

        case state::idle:
        case state::connecting:
        default:
            return false;
    }
}

void client::process_data() {
    auto result_opt = m_reader.process();
    if (!result_opt.has_value()) {
        return;
    }

    auto result = result_opt.value();
    if (result.error != client_reader::error_state::no_error) {
        // todo: handle
        return;
    }

    // todo: if index is old, give up on it? necessary?
    // todo: further parsing of message info
    switch (result.state.header.type) {
        case message_type::handshake:
            // todo: transfer data over handshake (use different message types)
            on_handshake_received();
            break;
        case message_type::entry_assign: {
            on_entry_assign(result.state.m_name, result.state.m_value);
            break;
        }
    }
}

void client::on_handshake_received() {
    switch (m_current_state) {
        case state::waiting_for_handshake:
            // we got handshake from server, respond and wait last ok
            write_handshake();
            m_current_state = state::waiting_for_go;
            break;
        case state::waiting_for_go:
            // finished handshake with server
            m_current_state = state::connected;
            break;
        case state::sent_initial_handshake:
            // finished merry-go-around with client
            write_handshake();
            m_current_state = state::connected;
            break;
        default:
            break;
    }
}

void client::on_entry_assign(std::string& name, value_t& value) {

}

void client::on_ready_resource(uint32_t flags) {
    std::unique_lock lock(m_mutex);

    if ((flags & obsr::os::selector::poll_in) != 0) {
        on_read_ready();
    }

    if ((flags & obsr::os::selector::poll_out) != 0) {
        on_write_ready();
    }
}

void client::on_read_ready() {
    switch (m_current_state) {
        case state::waiting_for_handshake:
        case state::waiting_for_go:
        case state::sent_initial_handshake:
        case state::connected:
            m_reader.update(m_socket.get());
            break;
        default:
            break;
    }
}

void client::on_write_ready() {
    switch (m_current_state) {
        case state::connecting:
            m_current_state = state::waiting_for_handshake;
            break;
        case state::waiting_for_handshake:
        case state::sent_initial_handshake:
        case state::connected:
            m_write_buffer.write_into(m_socket.get());
            break;
        default:
            break;
    }
}

bool client::on_open_socket() {
    m_socket = std::make_shared<obsr::os::socket>();

    try {
        m_socket->configure_blocking(false);
        m_socket->setoption<obsr::os::sockopt_reuseport>(true);

        m_resource_handle = m_nio_runner->add(m_socket,
                                              obsr::os::selector::poll_in | obsr::os::selector::poll_out,
                                              [this](uint32_t flags)->void { on_ready_resource(flags); });

        m_socket->connect(m_info.ip, m_info.port);

        return true;
    } catch (...) {
        free_resource();
        return false;
    }
}

void client::free_resource() {
    if (m_resource_handle != empty_handle) {
        m_nio_runner->remove(m_resource_handle);
        m_resource_handle = empty_handle;
    }

    m_socket.reset();
}*/

}
