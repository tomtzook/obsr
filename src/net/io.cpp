
#include "io/serialize.h"
#include "os/io.h"
#include "internal_except.h"
#include "debug.h"

#include "io.h"

namespace obsr::net {

// todo: organize better by dividing into smaller more managed units.
// todo: add logging
// todo: handle errors better by actually getting error info and passing it forward and such


#define LOG_MODULE "netclient"

template<typename listener_, typename... args_>
static void invoke_listener(std::unique_lock<std::mutex>& lock, listener_* listener_ref, void(listener_::*func)(args_...), args_... args) {
    if (listener_ref != nullptr) {
        auto listener = listener_ref;
        lock.unlock();
        try {
            (listener->*func)(args...);
        } catch (...) {}
        lock.lock();
    }
}


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

            // todo: eliminate this middleman
            if (!m_read_buffer.read(buffer, header.message_size)) {
                return error(read_error::read_failed);
            }

            return finished();
        }
        default:
            return error(read_error::read_unknown_state);
    }
}

socket_io::socket_io(std::shared_ptr<obsr::io::nio_runner> nio_runner, listener* listener)
    : m_nio_runner(std::move(nio_runner))
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
        stop_internal(lock);
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
        TRACE_DEBUG(LOG_MODULE, "start failed");
        stop_internal(lock);

        throw;
    }
}

void socket_io::stop() {
    std::unique_lock lock(m_mutex);

    if (m_state == state::idle) {
        throw illegal_state_exception();
    }

    stop_internal(lock);
}

void socket_io::connect(connection_info info) {
    std::unique_lock lock(m_mutex);

    if (m_state == state::idle || m_state == state::connecting || m_state == state::connected) {
        throw illegal_state_exception();
    }

    // remove in flags since we can't read while connecting
    m_nio_runner->remove_flags(m_resource_handle, obsr::os::selector::poll_in);
    try {
        m_state = state::connecting;
        m_socket->connect(info.ip, info.port);
    } catch (const io_exception&) {
        TRACE_DEBUG(LOG_MODULE, "connect failed");
        stop_internal(lock); // todo: maybe don't close socket

        throw;
    }

    m_nio_runner->add_flags(m_resource_handle, obsr::os::selector::poll_out);
}

bool socket_io::write(uint8_t type, const uint8_t* buffer, size_t size) {
    std::unique_lock lock(m_mutex);

    auto index = m_next_message_index++;
    message_header header {
            message_header::message_magic,
            message_header::current_version,
            index,
            type,
            static_cast<uint32_t>(size) //todo: return error if size too great
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
            stop_internal(lock);
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

void socket_io::stop_internal(std::unique_lock<std::mutex>& lock) {
    if (m_state == state::idle) {
        return;
    }

    TRACE_DEBUG(LOG_MODULE, "stop called");

    try {
        if (m_resource_handle != empty_handle) {
            m_nio_runner->remove(m_resource_handle);
        }
    } catch (...) {
        TRACE_DEBUG(LOG_MODULE, "error while detaching from nio");
    }

    try {
        m_socket->close();
        m_socket.reset();
    } catch (...) {
        TRACE_DEBUG(LOG_MODULE, "error while closing socket");
    }

    m_state = state::idle;

    invoke_listener(lock, m_listener, &listener::on_close);
}

socket_io::update_handler::update_handler(socket_io& io)
    : m_io(io)
    , m_lock(io.m_mutex)
{}

void socket_io::update_handler::on_read_ready() {
    TRACE_DEBUG(LOG_MODULE, "on read update");

    const auto state = m_io.m_state;
    if (state == state::connected) {
        try {
            m_io.m_reader.update(m_io.m_socket.get());
        } catch (const eof_exception&) {
            // socket was closed
            TRACE_DEBUG(LOG_MODULE, "read eof");
            m_io.stop_internal(m_lock); // todo: need more info on why we closed
        } catch (...) {
            // any other error
            TRACE_DEBUG(LOG_MODULE, "read error");
            m_io.stop_internal(m_lock);
        }

        // todo: is calling this here too heavy on the poll thread?
        process_new_data();
    } else {
        // we shouldn't be here
        m_io.m_nio_runner->remove_flags(m_io.m_resource_handle, obsr::os::selector::poll_in);
    }
}

void socket_io::update_handler::on_write_ready() {
    TRACE_DEBUG(LOG_MODULE, "on write update");

    const auto state = m_io.m_state;
    if (state == state::connecting) {
        TRACE_DEBUG(LOG_MODULE, "connect finished");

        try {
            m_io.m_socket->finalize_connect();
        } catch (const io_exception&) {
            // connect failed
            TRACE_DEBUG(LOG_MODULE, "connect failed");
            m_io.stop_internal(m_lock);

            return;
        }

        m_io.m_state = state::connected;
        // we can start reading again
        m_io.m_nio_runner->add_flags(m_io.m_resource_handle, obsr::os::selector::poll_in);

        invoke_listener(m_lock, m_io.m_listener, &listener::on_connected);
    } else if (state == state::connected) {
        try {
            TRACE_DEBUG(LOG_MODULE, "writing to socket");
            if (!m_io.m_write_buffer.write_into(m_io.m_socket.get())) {
                TRACE_DEBUG(LOG_MODULE, "nothing more to write");
                // nothing more to write
                m_io.m_nio_runner->remove_flags(m_io.m_resource_handle, obsr::os::selector::poll_out);
            }
        } catch (const io_exception&) {
            TRACE_DEBUG(LOG_MODULE, "write error");
            m_io.stop_internal(m_lock);
        }
    } else {
        // we shouldn't be here
        m_io.m_nio_runner->remove_flags(m_io.m_resource_handle, obsr::os::selector::poll_out);
    }
}

void socket_io::update_handler::on_hung_or_error() {
    m_io.stop_internal(m_lock);
}

void socket_io::update_handler::process_new_data() {
    bool run;
    do {
        run = false;
        m_io.m_reader.process();

        if (m_io.m_reader.is_errored()) {
            TRACE_DEBUG(LOG_MODULE, "read process error %d", m_io.m_reader.error_code());
            m_io.stop_internal(m_lock);
        } else if (m_io.m_reader.is_finished()) {
            TRACE_DEBUG(LOG_MODULE, "new message processed");
            auto& state = m_io.m_reader.data();

            invoke_listener<socket_io::listener, const message_header&, const uint8_t*, size_t>(
                    m_lock,
                    m_io.m_listener,
                    &listener::on_new_message,
                    state.header,
                    state.message_buffer,
                    state.header.message_size);

            m_io.m_reader.reset();

            // read one message, there might be another
            run = true;
        }
    } while (run);
}

server_io::server_io(std::shared_ptr<obsr::io::nio_runner> nio_runner, listener* listener)
    : m_nio_runner(std::move(nio_runner))
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
        stop_internal(lock);
    }
}

bool server_io::is_stopped() {
    std::unique_lock lock(m_mutex);

    return m_state == state::idle;
}

void server_io::start(int bind_port) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::idle) {
        throw illegal_state_exception();
    }

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
        TRACE_DEBUG(LOG_MODULE, "start failed");
        stop_internal(lock);

        throw;
    }
}

void server_io::stop() {
    std::unique_lock lock(m_mutex);

    if (m_state == state::idle) {
        throw illegal_state_exception();
    }

    stop_internal(lock);
}

void server_io::write_to(uint32_t id, uint8_t type, const uint8_t* buffer, size_t size) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::open) {
        throw illegal_state_exception();
    }

    auto it = m_clients.find(id);
    if (it == m_clients.end()) {
        throw illegal_state_exception(); // todo: throw no such server exception
    }

    it->second->write(type, buffer, size);
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

void server_io::stop_internal(std::unique_lock<std::mutex>& lock) {
    if (m_state == state::idle) {
        return;
    }

    TRACE_DEBUG(LOG_MODULE, "stop called");

    try {
        if (m_resource_handle != empty_handle) {
            m_nio_runner->remove(m_resource_handle);
        }
    } catch (...) {
        TRACE_DEBUG(LOG_MODULE, "error while detaching from nio");
    }

    try {
        m_socket->close();
        m_socket.reset();
    } catch (...) {
        TRACE_DEBUG(LOG_MODULE, "error while closing socket");
    }

    m_state = state::idle;

    invoke_listener(lock, m_listener, &listener::on_close);
}

void server_io::on_client_connected(uint32_t id) {
    std::unique_lock lock(m_mutex);
    invoke_listener(lock, m_listener, &server_io::listener::on_client_connected, id);
}

void server_io::on_client_disconnected(uint32_t id) {
    std::unique_lock lock(m_mutex);
    invoke_listener(lock, m_listener, &server_io::listener::on_client_disconnected, id);
}

void server_io::on_new_client_data(uint32_t id, const message_header& header, const uint8_t* buffer, size_t size) {
    std::unique_lock lock(m_mutex);
    invoke_listener<server_io::listener, uint32_t, const message_header&, const uint8_t*, size_t>(
            lock,
            m_listener,
            &server_io::listener::on_new_message,
            id,
            header,
            buffer,
            size);
}

server_io::update_handler::update_handler(server_io& io)
    : m_io(io)
    , m_lock(io.m_mutex)
{}

void server_io::update_handler::on_read_ready() {
    TRACE_DEBUG(LOG_MODULE, "on read ready");

    uint32_t id = -1;
    try {
        auto socket = m_io.m_socket->accept();

        id = m_io.m_next_client_id++;
        TRACE_DEBUG(LOG_MODULE, "handling new server %d", id);

        auto client = std::make_unique<client_data>(m_io, id);
        auto [it, inserted] = m_io.m_clients.emplace(id, std::move(client));
        if (!inserted) {
            TRACE_DEBUG(LOG_MODULE, "failed to store new client");
            socket->close();
            return;
        }

        it->second->attach(std::move(socket));

        TRACE_DEBUG(LOG_MODULE, "new server registered %d", id);
    } catch (const io_exception&) {
        TRACE_DEBUG(LOG_MODULE, "error with new server");

        if (id != -1) {
            m_io.m_clients.erase(id);
        }
    }
}

void server_io::update_handler::on_hung_or_error() {
    m_io.stop_internal(m_lock);
}

server_io::client_data::client_data(server_io& parent, uint32_t id)
    : m_parent(parent)
    , m_id(id)
    , m_io(std::move(m_parent.m_nio_runner), this) {
}

void server_io::client_data::attach(std::unique_ptr<os::socket>&& socket) {
    std::shared_ptr<obsr::os::socket> socket_shared{std::move(socket)};

    m_io.start(socket_shared, true);
}

void server_io::client_data::write(uint8_t type, const uint8_t* buffer, size_t size) {
    m_io.write(type, buffer, size);
}

void server_io::client_data::on_new_message(const message_header& header, const uint8_t* buffer, size_t size)  {
    m_parent.on_new_client_data(m_id, header, buffer, size);
}

void server_io::client_data::on_connected() {
    m_parent.on_client_connected(m_id);
}

void server_io::client_data::on_close() {
    m_parent.on_client_disconnected(m_id);
}

}
