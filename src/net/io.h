#pragma once

#include <memory>
#include <mutex>

#include "os/socket.h"
#include "io/buffer.h"
#include "io/nio.h"
#include "util/state.h"

namespace obsr::net {

#pragma pack(push, 1)
struct message_header {
    static constexpr uint8_t message_magic = 0x29;
    static constexpr uint8_t current_version = 0x1;

    uint8_t magic;
    uint8_t version;
    uint32_t index;
    uint8_t type;
    uint32_t message_size;
};
#pragma pack(pop)

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

class reader : public state_machine<read_state, read_state::header, read_data> {
public:
    explicit reader(size_t buffer_size);

    void update(obsr::os::readable* readable);

protected:
    bool process_state(read_state current_state, read_data& data) override;

private:
    obsr::io::buffer m_read_buffer;
};

class socket_io {
public:
    class listener {
    public:
        virtual void on_new_message(const message_header& header, const uint8_t* buffer, size_t size) = 0;
        virtual void on_connected() = 0;
        virtual void on_close() = 0;
    };

    socket_io(std::shared_ptr<io::nio_runner> nio_runner, listener* listener);
    ~socket_io();

    bool is_stopped();

    void start(std::shared_ptr<obsr::os::socket> socket, bool connected = false);
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
    class update_handler {
    public:
        explicit update_handler(socket_io& io);

        void on_read_ready();
        void on_write_ready();
        void on_hung_or_error();

    private:
        void process_new_data();

        socket_io& m_io;
        std::unique_lock<std::mutex> m_lock;
    };

    void on_ready_resource(uint32_t flags);
    void stop_internal(std::unique_lock<std::mutex>& lock);

    std::shared_ptr<obsr::io::nio_runner> m_nio_runner;
    obsr::handle m_resource_handle;

    std::shared_ptr<obsr::os::socket> m_socket;
    std::mutex m_mutex; // todo: seperate mutex for read/write maybe

    reader m_reader;
    obsr::io::buffer m_write_buffer;
    state m_state;
    uint32_t m_next_message_index;

    listener* m_listener;
};

class server_io {
public:
    class listener {
    public:
        virtual void on_client_connected(uint32_t id) = 0;
        virtual void on_client_disconnected(uint32_t id) = 0;
        virtual void on_new_message(uint32_t id, const message_header& header, const uint8_t* buffer, size_t size) = 0;
        virtual void on_close() = 0;
    };

    server_io(std::shared_ptr<obsr::io::nio_runner> nio_runner, listener* listener);
    ~server_io();

    bool is_stopped();

    void start(int bind_port);
    void stop();

    void write_to(uint32_t id, uint8_t type, const uint8_t* buffer, size_t size);

private:
    class update_handler {
    public:
        explicit update_handler(server_io& io);

        void on_read_ready();
        void on_hung_or_error();

    private:
        server_io& m_io;
        std::unique_lock<std::mutex> m_lock;
    };
    struct client_data : public socket_io::listener { // todo: hide these inner classes??
    public:
        client_data(server_io& parent, uint32_t id);

        void attach(std::unique_ptr<os::socket>&& socket);
        void write(uint8_t type, const uint8_t* buffer, size_t size);

        // events from server
        void on_new_message(const message_header& header, const uint8_t* buffer, size_t size) override;
        void on_connected() override;
        void on_close() override;

    private:
        server_io& m_parent;
        uint32_t m_id;
        socket_io m_io;
    };
    enum class state {
        idle,
        open
    };

    void on_ready_resource(uint32_t flags);
    void stop_internal(std::unique_lock<std::mutex>& lock);

    void on_client_connected(uint32_t id);
    void on_client_disconnected(uint32_t id);
    void on_new_client_data(uint32_t id, const message_header& header, const uint8_t* buffer, size_t size);

    std::shared_ptr<obsr::io::nio_runner> m_nio_runner;
    obsr::handle m_resource_handle;

    std::shared_ptr<obsr::os::server_socket> m_socket;
    std::mutex m_mutex; // todo: seperate mutex for read/write maybe

    std::unordered_map<uint32_t, std::unique_ptr<client_data>> m_clients;
    uint32_t m_next_client_id;
    state m_state;

    listener* m_listener;
};


// todo: need base server to be shared by
//  actual clients for server processes
//  server clients in server processes
// todo: consider how to initialize new entry from name to id
// todo: how to do handshake where server tells us of existing entries?
// todo: comm-storage data transfer

}
