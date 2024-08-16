
#include "io/serialize.h"
#include "os/io.h"
#include "internal_except.h"
#include "debug.h"

#include "client.h"

namespace obsr::net {

// todo: organize better by dividing into smaller more managed units.
// todo: add logging
// todo: handle errors better by actually getting error info and passing it forward and such


#define LOG_MODULE "netclient"

client_reader::client_reader(size_t buffer_size)
    : state_machine()
    , m_read_buffer(buffer_size) {
}

void client_reader::update(obsr::os::readable* readable) {
    m_read_buffer.read_from(readable);
}

bool client_reader::process_state(client_read_state current_state, client_read_data& data) {
    switch (current_state) {
        case client_read_state::header: {
            auto& header = data.header;
            do {
                // todo: optimization for locating magic
                auto success = m_read_buffer.read(header);
                if (!success) {
                    return try_later();
                }
            } while (header.magic != message_header::message_magic ||
                     header.version != message_header::current_version);

            return move_to_state(client_read_state::message);
        }
        case client_read_state::message: {
            const auto& header = data.header;
            auto buffer = data.message_buffer;
            if (header.message_size > client_read_data::message_buffer_size) {
                // we can skip forward by the size, but regardless we will handle it fine because
                // we jump to the magic
                m_read_buffer.seek_read(header.message_size);
                return error(client_read_error::read_unsupported_size);
            }

            if (!m_read_buffer.can_read(header.message_size)) {
                return try_later();
            }

            // todo: eliminate this middleman
            if (!m_read_buffer.read(buffer, header.message_size)) {
                return error(client_read_error::read_failed);
            }

            return finished();
        }
        default:
            return error(client_read_error::read_unknown_state);
    }
}

client_io::client_io(std::shared_ptr<obsr::io::nio_runner> nio_runner, listener* listener)
    : m_nio_runner(std::move(nio_runner))
    , m_resource_handle(empty_handle)
    , m_socket()
    , m_reader(1024)
    , m_write_buffer(1024)
    , m_message_index(0)
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

    if (m_closed) {
        throw illegal_state_exception();
    }

    m_reader.reset();
    m_write_buffer.reset();
    m_message_index = 0;
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
        stop_internal(lock);
    }
}

void client_io::stop() {
    std::unique_lock lock(m_mutex);
    stop_internal(lock);
}

void client_io::connect(connection_info info) {
    std::unique_lock lock(m_mutex);

    m_connecting = true;
    m_socket->connect(info.ip, info.port);

    m_nio_runner->add_flags(m_resource_handle, obsr::os::selector::poll_out);
}

bool client_io::write(uint8_t type, uint8_t* buffer, size_t size) {
    std::unique_lock lock(m_mutex);

    auto index = m_message_index++;
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

void client_io::on_ready_resource(uint32_t flags) {
    update_handler handler(*this);

    if ((flags & obsr::os::selector::poll_in) != 0) {
        handler.on_read_ready();
    }

    if ((flags & obsr::os::selector::poll_out) != 0) {
        handler.on_write_ready();
    }
}

void client_io::stop_internal(std::unique_lock<std::mutex>& lock) {
    if (m_closed) {
        return;
    }

    TRACE_DEBUG(LOG_MODULE, "stop called");

    try {
        if (m_resource_handle != empty_handle) {
            m_nio_runner->remove(m_resource_handle);
        }

        m_socket->close();
        m_socket.reset();
    } catch (...) {
        TRACE_DEBUG(LOG_MODULE, "error while stopping");
    }

    if (m_listener != nullptr) {
        auto listener = m_listener;
        lock.unlock();
        try {
            listener->on_close();
        } catch (...) {}
        lock.lock();
    }

    m_closed = true;
}

client_io::update_handler::update_handler(client_io& io)
    : m_io(io)
    , m_lock(io.m_mutex)
{}

void client_io::update_handler::on_read_ready() {
    TRACE_DEBUG(LOG_MODULE, "on read update");
    try {
        m_io.m_reader.update(m_io.m_socket.get());
    } catch (const eof_exception&) {
        // socket was closed
        TRACE_DEBUG(LOG_MODULE, "read eof");
        m_io.stop_internal(m_lock);
    } catch (...) {
        // any other error
        TRACE_DEBUG(LOG_MODULE, "read error");
        m_io.stop_internal(m_lock);
    }

    // todo: is calling this here too heavy on the poll thread?
    process_new_data();
}

void client_io::update_handler::on_write_ready() {
    TRACE_DEBUG(LOG_MODULE, "on write update");
    if (m_io.m_connecting) {
        TRACE_DEBUG(LOG_MODULE, "connect finished");
        m_io.m_connecting = false;

        if (m_io.m_listener != nullptr) {
            auto listener = m_io.m_listener;
            m_lock.unlock();
            try {
                listener->on_connected();
            } catch (...) {}
            m_lock.lock();
        }
    } else {
        try {
            TRACE_DEBUG(LOG_MODULE, "writing to socket");
            if (!m_io.m_write_buffer.write_into(m_io.m_socket.get())) {
                TRACE_DEBUG(LOG_MODULE, "nothing more to write");
                // nothing more to write
                m_io.m_nio_runner->remove_flags(m_io.m_resource_handle, obsr::os::selector::poll_out);
            }
        } catch (...) {
            TRACE_DEBUG(LOG_MODULE, "write error");
            m_io.stop_internal(m_lock);
        }
    }
}

void client_io::update_handler::process_new_data() {
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

            if (m_io.m_listener != nullptr) {
                auto listener = m_io.m_listener;
                m_lock.unlock();
                try {
                    listener->on_new_message(state.header, state.message_buffer, state.header.message_size);
                } catch (...) {}
                m_lock.lock();
            }

            m_io.m_reader.reset();

            // read one message, there might be another
            run = true;
        }
    } while (run);
}

}
