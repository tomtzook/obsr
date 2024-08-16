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

struct client_read_data {
    static constexpr size_t message_buffer_size = 1024;
    message_header header;
    uint8_t message_buffer[message_buffer_size];
};

enum class client_read_state {
    header,
    message
};

enum client_read_error {
    read_unsupported_size = 1,
    read_unknown_state = 2,
    read_failed = 3
};

class client_reader : public state_machine<client_read_state, client_read_state::header, client_read_data> {
public:
    explicit client_reader(size_t buffer_size);

    void update(obsr::os::readable* readable);

protected:
    bool process_state(client_read_state current_state, client_read_data& data) override;

private:
    obsr::io::buffer m_read_buffer;
};

class client_io {
public:
    class listener {
    public:
        virtual void on_new_message(const message_header& header, const uint8_t* buffer, size_t size) = 0;
        virtual void on_connected() = 0;
        virtual void on_close() = 0;
    };

    client_io(std::shared_ptr<io::nio_runner> nio_runner, listener* listener);
    ~client_io();

    bool is_closed();

    void start(std::shared_ptr<obsr::os::socket> socket);
    void stop();

    void connect(connection_info info);

    bool write(uint8_t type, uint8_t* buffer, size_t size);

private:
    class update_handler {
    public:
        explicit update_handler(client_io& io);

        void on_read_ready();
        void on_write_ready();

    private:
        void process_new_data();

        client_io& m_io;
        std::unique_lock<std::mutex> m_lock;
    };

    void on_ready_resource(uint32_t flags);
    void stop_internal(std::unique_lock<std::mutex>& lock);

    std::shared_ptr<obsr::io::nio_runner> m_nio_runner;
    obsr::handle m_resource_handle;

    std::shared_ptr<obsr::os::socket> m_socket;
    std::mutex m_mutex; // todo: seperate mutex for read/write maybe

    client_reader m_reader;
    obsr::io::buffer m_write_buffer;
    bool m_connecting;
    bool m_closed;
    uint32_t m_message_index;

    listener* m_listener;

    friend class update_handler;
};

}
