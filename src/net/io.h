#pragma once

#include <memory>
#include <mutex>

#include "os/socket.h"
#include "io/buffer.h"
#include "util/state.h"
#include "net/serialize.h"
#include "events/events.h"

namespace obsr::net {

struct connection_info {
    std::string_view ip;
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

class reader : public state_machine<read_state, read_state::header, read_data> {
public:
    explicit reader(size_t buffer_size);

    bool update(obsr::os::readable* readable);

protected:
    bool process_state(read_state current_state, read_data& data) override;

private:
    obsr::io::buffer m_read_buffer;
};

// must be used from inside the looper
class socket_io {
public:
    class listener {
    public:
        virtual void on_new_message(const message_header& header, const uint8_t* buffer, size_t size) = 0;
        virtual void on_connected() = 0;
        virtual void on_close() = 0;
    };

    socket_io();
    ~socket_io();

    void start(events::looper* looper,
               listener* listener);
    void start(events::looper* looper,
               listener* listener,
               std::shared_ptr<obsr::os::socket> socket,
               bool connected = false);
    void stop();

    void connect(connection_info info);
    bool write(uint8_t type, const uint8_t* buffer, size_t size);

private:
    enum class state {
        idle,
        bound,
        connecting,
        connected
    };

    void on_read_ready();
    void on_write_ready();
    void on_hung_or_error();
    void process_new_data();

    void stop_internal();

    state m_state;
    listener* m_listener;
    events::looper* m_looper;
    obsr::handle m_looper_handle;

    std::shared_ptr<obsr::os::socket> m_socket;
    reader m_reader;
    obsr::io::buffer m_write_buffer;
    uint32_t m_next_message_index;
};

class server_io {
public:
    using client_id = uint16_t;
    static constexpr client_id invalid_client_id = static_cast<client_id>(-1);

    class listener {
    public:
        virtual void on_client_connected(client_id id) = 0;
        virtual void on_client_disconnected(client_id id) = 0;
        virtual void on_new_message(client_id id, const message_header& header, const uint8_t* buffer, size_t size) = 0;
        virtual void on_close() = 0;
    };

    server_io();
    ~server_io();

    void start(events::looper* looper, listener* listener, uint16_t bind_port);
    void stop();

    bool write_to(client_id id, uint8_t type, const uint8_t* buffer, size_t size);

private:
    enum class state {
        idle,
        open
    };
    struct client : public socket_io::listener {
    public:
        client(server_io& parent, client_id id);

        void start(events::looper* looper, std::shared_ptr<obsr::os::socket> socket);
        void stop();

        bool write(uint8_t type, const uint8_t* buffer, size_t size);

        // from listener
        void on_new_message(const message_header& header, const uint8_t* buffer, size_t size) override;
        void on_connected() override;
        void on_close() override;

    private:
        server_io& m_parent;
        client_id m_id;
        socket_io m_io;
        bool m_closing;
    };

    void on_read_ready();
    void on_hung_or_error();

    void stop_internal();

    state m_state;
    listener* m_listener;
    events::looper* m_looper;
    obsr::handle m_looper_handle;

    std::shared_ptr<obsr::os::server_socket> m_socket;

    std::unordered_map<client_id, std::unique_ptr<client>> m_clients;
    client_id m_next_client_id;
};

}
