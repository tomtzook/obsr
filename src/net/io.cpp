
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

bool reader::update(obsr::os::readable* readable) {
    return m_read_buffer.read_from(readable);
}

bool reader::process_state(read_state current_state, read_data& data) {
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

socket_io::socket_io()
    : m_state(state::idle)
    , m_looper(nullptr)
    , m_looper_handle(empty_handle)
    , m_callbacks()
    , m_socket()
    , m_reader(1024)
    , m_write_buffer(1024)
    , m_next_message_index(0)
{}

socket_io::~socket_io() {
    if (m_state != state::idle) {
        std::abort();
    }
}

void socket_io::on_connect(on_connect_cb callback) {
    m_callbacks.on_connect = std::move(callback);
}

void socket_io::on_close(on_close_cb callback) {
    m_callbacks.on_close = std::move(callback);
}

void socket_io::on_message(on_message_cb callback) {
    m_callbacks.on_message = std::move(callback);
}

void socket_io::start(events::looper* looper) {
    if (m_state != state::idle) {
        throw illegal_state_exception();
    }

    auto socket = std::make_shared<obsr::os::socket>();
    try {
        socket->setoption<os::sockopt_reuseport>(true);
        socket->configure_blocking(false);
    } catch (const io_exception& e) {
        TRACE_ERROR(LOG_MODULE_CLIENT, "failed creating socket: code=%d", e.get_code());

        socket->close();
        socket.reset();

        throw;
    }

    start(looper, socket, false);
}

void socket_io::start(events::looper* looper,
                      std::shared_ptr<obsr::os::socket> socket,
                      bool connected) {
    if (m_state != state::idle) {
        throw illegal_state_exception();
    }

    m_looper = looper;

    m_socket = std::move(socket);
    m_socket->configure_blocking(false);

    m_state = state::bound;

    events::event_types events = events::event_hung | events::event_error;
    if (connected) {
        events |= events::event_in | events::event_out;
        m_state = state::connected;
    }

    auto callback = [this](events::looper& looper, obsr::handle handle, events::event_types events)->void {
        if ((events & (events::event_hung | events::event_error)) != 0) {
            on_hung_or_error();
            return;
        }

        if ((events & events::event_in) != 0) {
            on_read_ready();
        }

        if ((events & events::event_out) != 0) {
            on_write_ready();
        }
    };

    m_looper_handle = m_looper->add(m_socket, events, callback);
}

void socket_io::stop() {
    if (m_state == state::idle) {
        return;
    }

    stop_internal(false);
}

void socket_io::connect(const connection_info& info) {
    if (m_state == state::idle || m_state == state::connecting || m_state == state::connected) {
        throw illegal_state_exception();
    }

    // remove in flags since we can't read while connecting
    m_looper->request_updates(m_looper_handle, events::event_in, events::looper::events_update_type::remove);
    try {
        m_state = state::connecting;
        m_socket->connect(info.ip, info.port);
    } catch (const io_exception& e) {
        TRACE_DEBUG(LOG_MODULE_CLIENT, "connect failed: code=%d", e.get_code());
        stop_internal();

        throw;
    }

    m_looper->request_updates(m_looper_handle, events::event_out, events::looper::events_update_type::append);
}

bool socket_io::write(uint8_t type, const uint8_t* buffer, size_t size) {
    if (!m_write_buffer.can_write(sizeof(message_header) + size)) {
        TRACE_DEBUG(LOG_MODULE_CLIENT, "write buffer does not have enough space");
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
            // this means we have probably sent a message with a header but no data. this will seriously
            // break down communication. as such, we will terminate connection here.
            TRACE_ERROR(LOG_MODULE_CLIENT, "write attempt failed halfway, stopping");
            stop_internal();
            return false;
        }
    }

    m_looper->request_updates(m_looper_handle, events::event_out, events::looper::events_update_type::append);

    return true;
}

void socket_io::on_read_ready() {
    TRACE_DEBUG(LOG_MODULE_CLIENT, "on read update");

    if (m_state == state::connected) {
        try {
            m_reader.update(m_socket.get());
        } catch (const eof_exception&) {
            // socket was closed
            TRACE_ERROR(LOG_MODULE_CLIENT, "read eof");
            stop_internal();
        } catch (...) {
            // any other error
            TRACE_ERROR(LOG_MODULE_CLIENT, "read error");
            stop_internal();
        }

        process_new_data();
    } else {
        // we shouldn't be here
        m_looper->request_updates(m_looper_handle, events::event_in, events::looper::events_update_type::remove);
    }
}

void socket_io::on_write_ready() {
    TRACE_DEBUG(LOG_MODULE_CLIENT, "on write update");

    if (m_state == state::connecting) {
        TRACE_INFO(LOG_MODULE_CLIENT, "connect finished");

        try {
            m_socket->finalize_connect();
        } catch (const io_exception& e) {
            // connect failed
            TRACE_ERROR(LOG_MODULE_CLIENT, "connect failed: code=%d", e.get_code());
            stop_internal();

            return;
        }

        m_state = state::connected;
        // we can start reading again
        m_looper->request_updates(m_looper_handle, events::event_in, events::looper::events_update_type::append);

        invoke_func_nolock(m_callbacks.on_connect);
    } else if (m_state == state::connected) {
        try {
            TRACE_DEBUG(LOG_MODULE_CLIENT, "writing to socket");
            if (!m_write_buffer.write_into(m_socket.get())) {
                TRACE_DEBUG(LOG_MODULE_CLIENT, "nothing more to write");
                // nothing more to write
                m_looper->request_updates(m_looper_handle, events::event_out, events::looper::events_update_type::remove);
            }
        } catch (const io_exception& e) {
            TRACE_ERROR(LOG_MODULE_CLIENT, "write error: code=%d", e.get_code());
            stop_internal();
        }
    } else {
        // we shouldn't be here
        m_looper->request_updates(m_looper_handle, events::event_out, events::looper::events_update_type::remove);
    }
}

void socket_io::on_hung_or_error() {
    TRACE_ERROR(LOG_MODULE_CLIENT, "received error/hung event. internal error=%d", m_socket->get_internal_error());
    stop_internal();
}

void socket_io::process_new_data() {
    bool run;
    do {
        run = false;
        m_reader.process();

        if (m_reader.is_errored()) {
            TRACE_ERROR(LOG_MODULE_CLIENT, "read update error %d", m_reader.error_code());
            stop_internal();
        } else if (m_reader.is_finished()) {
            auto& state = m_reader.data();
            TRACE_DEBUG(LOG_MODULE_CLIENT, "new message processed %d", state.header.index);

            invoke_func_nolock<const message_header&, const uint8_t*, size_t>(
                    m_callbacks.on_message,
                    state.header,
                    state.message_buffer,
                    state.header.message_size);

            m_reader.reset();

            // read one message, there might be another
            run = true;
        } else {
            TRACE_DEBUG(LOG_MODULE_CLIENT, "message processor didn't finish, try again when more data is received");
        }
    } while (run);
}

void socket_io::stop_internal(bool notify) {
    if (m_state == state::idle) {
        return;
    }

    TRACE_INFO(LOG_MODULE_CLIENT, "stop called");

    try {
        if (m_looper_handle != empty_handle) {
            m_looper->remove(m_looper_handle);
            m_looper_handle = empty_handle;
        }
    } catch (...) {
        TRACE_ERROR(LOG_MODULE_CLIENT, "error while detaching from looper");
    }

    try {
        m_socket->close();
        m_socket.reset();
    } catch (...) {
        TRACE_ERROR(LOG_MODULE_CLIENT, "error while closing socket");
    }

    m_state = state::idle;

    if (notify) {
        invoke_func_nolock(m_callbacks.on_close);
    }
}

server_io::server_io()
    : m_state(state::idle)
    , m_looper(nullptr)
    , m_looper_handle(empty_handle)
    , m_callbacks()
    , m_socket()
    , m_clients()
    , m_next_client_id(0)
{}

server_io::~server_io() {
    if (m_state != state::idle) {
        std::abort();
    }
}

void server_io::on_connect(on_connect_cb callback) {
    m_callbacks.on_connect = std::move(callback);
}

void server_io::on_disconnect(on_disconnect_cb callback) {
    m_callbacks.on_disconnect = std::move(callback);
}

void server_io::on_close(on_close_cb callback) {
    m_callbacks.on_close = std::move(callback);
}

void server_io::on_message(on_message_cb callback) {
    m_callbacks.on_message = std::move(callback);
}

void server_io::start(events::looper* looper, uint16_t bind_port) {
    if (m_state != state::idle) {
        throw illegal_state_exception();
    }

    m_looper = looper;

    TRACE_INFO(LOG_MODULE_SERVER, "start called");

    m_clients.clear();
    m_next_client_id = 0;

    try {
        m_socket = std::make_shared<obsr::os::server_socket>();
        m_socket->setoption<os::sockopt_reuseport>(true);
        m_socket->configure_blocking(false);
        m_socket->bind(bind_port);
        m_socket->listen(2);
    } catch (const io_exception& e) {
        TRACE_ERROR(LOG_MODULE_SERVER, "start failed: code=%d", e.get_code());

        if (m_socket) {
            m_socket->close();
            m_socket.reset();
        }

        throw;
    }

    m_state = state::open;

    events::event_types events = events::event_hung | events::event_error | events::event_in;
    auto callback = [this](events::looper& looper, obsr::handle handle, events::event_types events)->void {
        if ((events & (events::event_hung | events::event_error)) != 0) {
            on_hung_or_error();
            return;
        }

        if ((events & events::event_in) != 0) {
            on_read_ready();
        }
    };

    m_looper_handle = m_looper->add(m_socket, events, callback);
}

void server_io::stop() {
    if (m_state == state::idle) {
        throw illegal_state_exception();
    }

    stop_internal(false);
}

bool server_io::write_to(client_id id, uint8_t type, const uint8_t* buffer, size_t size) {
    if (m_state != state::open) {
        throw illegal_state_exception();
    }

    auto it = m_clients.find(id);
    if (it == m_clients.end()) {
        throw illegal_state_exception();
    }

    return it->second->write(type, buffer, size);
}

void server_io::on_read_ready() {
    TRACE_DEBUG(LOG_MODULE_SERVER, "on read ready");

    client_id id = invalid_client_id;
    try {
        auto socket = m_socket->accept();

        id = m_next_client_id++;
        TRACE_INFO(LOG_MODULE_SERVER, "handling new server client %d", id);

        auto client = std::make_unique<server_io::client>(*this, id);
        auto [it, inserted] = m_clients.emplace(id, std::move(client));
        if (!inserted) {
            TRACE_ERROR(LOG_MODULE_SERVER, "failed to store new client");
            socket->close();
            return;
        }

        it->second->start(m_looper, std::move(socket));
        invoke_func_nolock(m_callbacks.on_connect, id);

        TRACE_ERROR(LOG_MODULE_SERVER, "new client registered %d", id);
    } catch (const io_exception& e) {
        TRACE_ERROR(LOG_MODULE_SERVER, "error with new server: code=%d", e.get_code());

        if (id != invalid_client_id) {
            auto it = m_clients.find(id);
            if (it != m_clients.end()) {
                m_clients.erase(it);
            }
        }
    }
}

void server_io::on_hung_or_error() {
    TRACE_ERROR(LOG_MODULE_SERVER, "received error/hung event. internal error=%d", m_socket->get_internal_error());
    stop_internal();
}

void server_io::stop_internal(bool notify) {
    if (m_state == state::idle) {
        return;
    }

    TRACE_INFO(LOG_MODULE_SERVER, "stop called");

    try {
        if (m_looper_handle != empty_handle) {
            m_looper->remove(m_looper_handle);
            m_looper_handle = empty_handle;
        }
    } catch (...) {
        TRACE_ERROR(LOG_MODULE_SERVER, "error while detaching from looper");
    }

    for (auto& [id, client] : m_clients) {
        try {
            client->stop();
        } catch (...) {
            TRACE_ERROR(LOG_MODULE_SERVER, "error stopping client");
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

    if (notify) {
        invoke_func_nolock(m_callbacks.on_close);
    }
}

server_io::client::client(server_io& parent, client_id id)
    : m_parent(parent)
    , m_id(id)
    , m_io()
    , m_closing(false) {
    m_io.on_connect([this]()->void {
        invoke_func_nolock(
                m_parent.m_callbacks.on_connect,
                m_id);
    });
    m_io.on_close([this]()->void {
        invoke_func_nolock(
                m_parent.m_callbacks.on_disconnect,
                m_id);
    });
    m_io.on_message([this](const message_header& header, const uint8_t* buffer, size_t size)->void {
        invoke_func_nolock<client_id, const message_header&, const uint8_t*, size_t>(
                m_parent.m_callbacks.on_message,
                m_id,
                header,
                buffer,
                size);
    });
}

void server_io::client::start(events::looper* looper, std::shared_ptr<obsr::os::socket> socket) {
    m_io.start(looper, std::move(socket), true);
}

void server_io::client::stop() {
    m_closing = true;
    m_io.stop();
}

bool server_io::client::write(uint8_t type, const uint8_t* buffer, size_t size) {
    return m_io.write(type, buffer, size);
}

}
