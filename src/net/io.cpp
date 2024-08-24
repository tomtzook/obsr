
#include "io/serialize.h"
#include "os/io.h"
#include "internal_except.h"
#include "debug.h"
#include "util/general.h"

#include "io.h"

namespace obsr::net {

#define LOG_MODULE_CLIENT "socketio"
#define LOG_MODULE_SERVER "serverio"

reader::reader(size_t buffer_size)
    : state_machine()
    , m_read_buffer(buffer_size) {
}

void reader::update(obsr::os::readable* readable) {
    m_read_buffer.read_from(readable);
}

bool reader::process_state(read_state current_state, read_data& data) {
    switch (current_state) {
        case read_state::header: {
            auto& header = data.header;
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
            const auto& header = data.header;
            auto buffer = data.message_buffer;
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

socket_io::socket_io(const std::shared_ptr<io::nio_runner>& nio_runner, listener* listener)
    : m_nio_runner(nio_runner)
    , m_resource_handle(empty_handle)
    , m_socket()
    , m_reader(1024)
    , m_write_buffer(1024)
    , m_next_message_index(0)
    , m_state(state::idle)
    , m_listener(listener) {

}

socket_io::~socket_io() {
    std::unique_lock lock(m_mutex);
    if (m_state != state::idle) {
        stop_internal(lock, io_stop_reason::deconstructed, false);
    }
}

bool socket_io::is_stopped() {
    std::unique_lock lock(m_mutex);

    return m_state == state::idle;
}

void socket_io::start(std::shared_ptr<obsr::os::socket> socket, bool connected) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::idle) {
        throw illegal_state_exception();
    }

    TRACE_INFO(LOG_MODULE_CLIENT, "start called");

    m_reader.reset();
    m_write_buffer.reset();
    m_next_message_index = 0;
    m_resource_handle = empty_handle;

    try {
        m_socket = std::move(socket);
        m_socket->configure_blocking(false);

        m_state = state::bound;

        uint32_t flags = obsr::os::selector::poll_hung | obsr::os::selector::poll_error;
        if (connected) {
            flags |= obsr::os::selector::poll_in | obsr::os::selector::poll_out;
            m_state = state::connected;
        }

        m_resource_handle = m_nio_runner->add(m_socket,
                                              flags,
                                              [this](obsr::os::resource& res, uint32_t flags)->void { on_ready_resource(flags); });
    } catch (...) {
        TRACE_ERROR(LOG_MODULE_CLIENT, "start failed");
        stop_internal(lock, io_stop_reason::open_error);

        throw;
    }
}

void socket_io::stop() {
    std::unique_lock lock(m_mutex);

    if (m_state == state::idle) {
        throw illegal_state_exception();
    }

    stop_internal(lock, io_stop_reason::external_call, false);
}

void socket_io::connect(connection_info info) {
    std::unique_lock lock(m_mutex);

    if (m_state == state::idle || m_state == state::connecting || m_state == state::connected) {
        throw illegal_state_exception();
    }

    // remove_and_wait in flags since we can't read while connecting
    m_nio_runner->remove_flags(m_resource_handle, obsr::os::selector::poll_in);
    try {
        m_state = state::connecting;
        m_socket->connect(info.ip, info.port);
    } catch (const io_exception&) {
        TRACE_DEBUG(LOG_MODULE_CLIENT, "connect failed");
        stop_internal(lock, io_stop_reason::connect_failed);

        throw;
    }

    m_nio_runner->add_flags(m_resource_handle, obsr::os::selector::poll_out);
}

bool socket_io::write(uint8_t type, const uint8_t* buffer, size_t size) {
    std::unique_lock lock(m_mutex);

    if (!m_write_buffer.can_write(sizeof(message_header) + size)) {
        TRACE_DEBUG(LOG_MODULE_CLIENT, "writevalue buffer does not have enough space");
        return false;
    }

    auto index = m_next_message_index++;
    message_header header {
            message_header::message_magic,
            message_header::current_version,
            index,
            type,
            static_cast<uint32_t>(size)
    };

    if (!m_write_buffer.write(reinterpret_cast<uint8_t*>(&header), sizeof(header))) {
        TRACE_DEBUG(LOG_MODULE_CLIENT, "write failed to buffer at start");
        return false;
    }

    if (buffer != nullptr && size > 0) {
        if (!m_write_buffer.write(buffer, size)) {
            // this means we have probably sent a message with an header but no data. this will seriously
            // break down communication. as such, we will terminate connection here.
            // todo: add ability for remote to deal with this safely.
            TRACE_ERROR(LOG_MODULE_CLIENT, "write attempt failed halfway, stopping");
            stop_internal(lock, io_stop_reason::write_error);
            return false;
        }
    }

    m_nio_runner->add_flags(m_resource_handle, obsr::os::selector::poll_out);

    return true;
}

void socket_io::on_ready_resource(uint32_t flags) {
    update_handler handler(*this);

    if ((flags & (obsr::os::selector::poll_hung | obsr::os::selector::poll_error)) != 0) {
        handler.on_hung_or_error();
    }

    if ((flags & obsr::os::selector::poll_in) != 0) {
        handler.on_read_ready();
    }

    if ((flags & obsr::os::selector::poll_out) != 0) {
        handler.on_write_ready();
    }
}

void socket_io::stop_internal(std::unique_lock<std::mutex>& lock, io_stop_reason reason, bool report) {
    if (m_state == state::idle) {
        return;
    }

    TRACE_INFO(LOG_MODULE_CLIENT, "stop called for reason=%d", static_cast<uint8_t>(reason));

    if (m_resource_handle != empty_handle) {
        try {
            m_nio_runner->remove(m_resource_handle);
        } catch (...) {
            TRACE_ERROR(LOG_MODULE_CLIENT, "error while detaching from nio");
        }
        m_resource_handle = empty_handle;
    }

    try {
        m_socket->close();
        m_socket.reset();
    } catch (...) {
        TRACE_ERROR(LOG_MODULE_CLIENT, "error while closing socket");
    }

    m_state = state::idle;
    TRACE_INFO(LOG_MODULE_CLIENT, "stop finished");

    if (report) {
        invoke_ptr(lock, m_listener, &listener::on_close);
    }
}

socket_io::update_handler::update_handler(socket_io& io)
    : m_io(io)
    , m_lock(io.m_mutex)
{}

void socket_io::update_handler::on_read_ready() {
    TRACE_DEBUG(LOG_MODULE_CLIENT, "on read update");

    const auto state = m_io.m_state;
    if (state == state::connected) {
        try {
            m_io.m_reader.update(m_io.m_socket.get());
        } catch (const eof_exception&) {
            // socket was closed
            TRACE_ERROR(LOG_MODULE_CLIENT, "read eof");
            m_io.stop_internal(m_lock, io_stop_reason::read_eof);
        } catch (...) {
            // any other error
            TRACE_ERROR(LOG_MODULE_CLIENT, "read error");
            m_io.stop_internal(m_lock, io_stop_reason::read_error);
        }

        process_new_data();
    } else {
        // we shouldn't be here
        m_io.m_nio_runner->remove_flags(m_io.m_resource_handle, obsr::os::selector::poll_in);
    }
}

void socket_io::update_handler::on_write_ready() {
    TRACE_DEBUG(LOG_MODULE_CLIENT, "on writevalue update");

    const auto state = m_io.m_state;
    if (state == state::connecting) {
        TRACE_INFO(LOG_MODULE_CLIENT, "connect finished");

        try {
            m_io.m_socket->finalize_connect();
        } catch (const io_exception&) {
            // connect failed
            TRACE_ERROR(LOG_MODULE_CLIENT, "connect failed");
            m_io.stop_internal(m_lock, io_stop_reason::connect_failed);

            return;
        }

        m_io.m_state = state::connected;
        // we can start reading again
        m_io.m_nio_runner->add_flags(m_io.m_resource_handle, obsr::os::selector::poll_in);

        invoke_ptr(m_lock, m_io.m_listener, &listener::on_connected);
    } else if (state == state::connected) {
        try {
            TRACE_DEBUG(LOG_MODULE_CLIENT, "writing to socket");
            if (!m_io.m_write_buffer.write_into(m_io.m_socket.get())) {
                TRACE_DEBUG(LOG_MODULE_CLIENT, "nothing more to writevalue");
                // nothing more to writevalue
                m_io.m_nio_runner->remove_flags(m_io.m_resource_handle, obsr::os::selector::poll_out);
            }
        } catch (const io_exception&) {
            TRACE_ERROR(LOG_MODULE_CLIENT, "writevalue error");
            m_io.stop_internal(m_lock, io_stop_reason::write_error);
        }
    } else {
        // we shouldn't be here
        m_io.m_nio_runner->remove_flags(m_io.m_resource_handle, obsr::os::selector::poll_out);
    }
}

void socket_io::update_handler::on_hung_or_error() {
    TRACE_ERROR(LOG_MODULE_CLIENT, "received error/hung event. internal error=%d", m_io.m_socket->get_internal_error());
    m_io.stop_internal(m_lock, io_stop_reason::poll_error);
}

void socket_io::update_handler::process_new_data() {
    bool run;
    do {
        run = false;
        m_io.m_reader.process();

        if (m_io.m_reader.is_errored()) {
            TRACE_ERROR(LOG_MODULE_CLIENT, "read update error %d", m_io.m_reader.error_code());
            m_io.stop_internal(m_lock, io_stop_reason::reader_error);
        } else if (m_io.m_reader.is_finished()) {
            TRACE_DEBUG(LOG_MODULE_CLIENT, "new message processed");
            auto& state = m_io.m_reader.data();

            invoke_ptr<socket_io::listener, const message_header&, const uint8_t*, size_t>(
                    m_lock,
                    m_io.m_listener,
                    &listener::on_new_message,
                    state.header,
                    state.message_buffer,
                    state.header.message_size);

            m_io.m_reader.reset();

            // read one message, there might be another
            run = true;
        } else {
            TRACE_DEBUG(LOG_MODULE_CLIENT, "message processor didn't finish, try again when more data is received");
        }
    } while (run);
}

server_io::server_io(const std::shared_ptr<obsr::io::nio_runner>& nio_runner, listener* listener)
    : m_nio_runner(nio_runner)
    , m_resource_handle(empty_handle)
    , m_socket()
    , m_mutex()
    , m_clients()
    , m_next_client_id(0)
    , m_state(state::idle)
    , m_listener(listener) {

}
server_io::~server_io() {
    std::unique_lock lock(m_mutex);
    if (m_state != state::idle) {
        stop_internal(lock, io_stop_reason::deconstructed, false);
    }
}

bool server_io::is_stopped() {
    std::unique_lock lock(m_mutex);

    return m_state == state::idle;
}

void server_io::start(uint16_t bind_port) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::idle) {
        throw illegal_state_exception();
    }

    TRACE_INFO(LOG_MODULE_SERVER, "start called");

    m_clients.clear();
    m_next_client_id = 0;

    try {
        m_socket = std::make_shared<obsr::os::server_socket>();
        m_socket->setoption<os::sockopt_reuseport>(true);
        m_socket->configure_blocking(false);
        m_socket->bind(bind_port);
        m_socket->listen(2);

        m_state = state::open;

        uint32_t flags = obsr::os::selector::poll_hung | obsr::os::selector::poll_error | obsr::os::selector::poll_in;
        m_resource_handle = m_nio_runner->add(m_socket,
                                              flags,
                                              [this](obsr::os::resource& res, uint32_t flags)->void { on_ready_resource(flags); });
    } catch (...) {
        TRACE_ERROR(LOG_MODULE_CLIENT, "start failed");
        stop_internal(lock, io_stop_reason::open_error);

        throw;
    }
}

void server_io::stop() {
    std::unique_lock lock(m_mutex);

    if (m_state == state::idle) {
        throw illegal_state_exception();
    }

    stop_internal(lock, io_stop_reason::external_call, false);
}

bool server_io::write_to(client_id id, uint8_t type, const uint8_t* buffer, size_t size) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::open) {
        throw illegal_state_exception();
    }

    auto it = m_clients.find(id);
    if (it == m_clients.end()) {
        throw illegal_state_exception();
    }

    return it->second->write(type, buffer, size);
}

void server_io::on_ready_resource(uint32_t flags) {
    update_handler handler(*this);

    if ((flags & (obsr::os::selector::poll_hung | obsr::os::selector::poll_error)) != 0) {
        handler.on_hung_or_error();
    }

    if ((flags & obsr::os::selector::poll_in) != 0) {
        handler.on_read_ready();
    }
}

void server_io::stop_internal(std::unique_lock<std::mutex>& lock, io_stop_reason reason, bool report) {
    if (m_state == state::idle) {
        return;
    }

    TRACE_INFO(LOG_MODULE_SERVER, "stop called for reason=%d", static_cast<uint8_t>(reason));

    if (m_resource_handle != empty_handle) {
        try {
            m_nio_runner->remove(m_resource_handle);
        } catch (...) {
            TRACE_ERROR(LOG_MODULE_SERVER, "error while detaching from nio");
        }
        m_resource_handle = empty_handle;
    }

    for (auto& [id, client] : m_clients) {
        try {
            client->stop();
        } catch (...) {
            TRACE_ERROR(LOG_MODULE_SERVER, "error stopping client id=%d", id);
        }
    }
    m_clients.clear();

    try {
        m_socket->close();
        m_socket.reset();
    } catch (...) {
        TRACE_ERROR(LOG_MODULE_SERVER, "error while closing socket");
    }

    m_state = state::idle;
    TRACE_INFO(LOG_MODULE_SERVER, "stop finished");

    if (report) {
        invoke_ptr(lock, m_listener, &listener::on_close);
    }
}

server_io::update_handler::update_handler(server_io& io)
    : m_io(io)
    , m_lock(io.m_mutex)
{}

void server_io::update_handler::on_read_ready() {
    TRACE_DEBUG(LOG_MODULE_SERVER, "on read ready");

    client_id id = -1;
    try {
        auto socket = m_io.m_socket->accept();

        id = m_io.m_next_client_id++;
        TRACE_INFO(LOG_MODULE_SERVER, "handling new server client %d", id);

        auto client = std::make_unique<client_data>(m_io, id);
        auto [it, inserted] = m_io.m_clients.emplace(id, std::move(client));
        if (!inserted) {
            TRACE_ERROR(LOG_MODULE_SERVER, "failed to store new client");
            socket->close();
            return;
        }

        it->second->attach(std::move(socket));
        invoke_ptr(m_lock, m_io.m_listener, &server_io::listener::on_client_connected, id);

        TRACE_ERROR(LOG_MODULE_SERVER, "new server registered %d", id);
    } catch (const io_exception&) {
        TRACE_ERROR(LOG_MODULE_SERVER, "error with new server");

        if (id != -1) {
            auto it = m_io.m_clients.find(id);
            if (it != m_io.m_clients.end()) {
                m_io.m_clients.erase(it);
            }
        }
    }
}

void server_io::update_handler::on_hung_or_error() {
    TRACE_ERROR(LOG_MODULE_SERVER, "received error/hung event. internal error=%d", m_io.m_socket->get_internal_error());
    m_io.stop_internal(m_lock, io_stop_reason::poll_error);
}

server_io::client_data::client_data(server_io& parent, client_id id)
    : m_parent(parent)
    , m_id(id)
    , m_io(m_parent.m_nio_runner, this) {
}

void server_io::client_data::attach(std::unique_ptr<os::socket>&& socket) {
    std::shared_ptr<obsr::os::socket> socket_shared{std::move(socket)};

    m_io.start(socket_shared, true);
}

bool server_io::client_data::write(uint8_t type, const uint8_t* buffer, size_t size) {
    return m_io.write(type, buffer, size);
}

void server_io::client_data::stop() {
    m_io.stop();
}

void server_io::client_data::on_new_message(const message_header& header, const uint8_t* buffer, size_t size)  {
    std::unique_lock lock(m_parent.m_mutex);
    invoke_ptr<server_io::listener, client_id, const message_header&, const uint8_t*, size_t>(
            lock,
            m_parent.m_listener,
            &server_io::listener::on_new_message,
            m_id,
            header,
            buffer,
            size);
}

void server_io::client_data::on_connected() {
    // not used as we already initialize it as connected
}

void server_io::client_data::on_close() {
    std::unique_lock lock(m_parent.m_mutex);
    invoke_ptr(lock, m_parent.m_listener, &server_io::listener::on_client_disconnected, m_id);
}

}
