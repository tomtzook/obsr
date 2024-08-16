#pragma once

#include <memory>
#include <mutex>

#include "os/socket.h"
#include "io/buffer.h"
#include "io/nio.h"

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

/*enum class message_type : uint32_t {
    handshake = 1,
    entry_assign,
    entry_update,
    entry_delete
};*/

struct connection_info {
    std::string ip;
    uint16_t port;
};

/*class client_reader_old {
public:
    struct state {
        message_header m_header;
        size_t m_name_size;
        std::string m_name;
        obsr::value_type m_type;
        obsr::value_t m_value;
    };
    enum class error_state {
        no_error,
        unknown_message_type,
        unknown_state
    };
    struct result {
        error_state error;
        state state;
    };

    client_reader_old();

    void reset();
    void update(obsr::os::readable* readable);
    std::optional<result> process();

private:
    enum class read_state {
        start,
        error,
        end,
        entryassign_1namesize,
        entryassign_2name,
        entryassign_3type,
        entryassign_4value,
        entryupdate_1namesize,
        entryupdate_2name,
        entryupdate_3type,
        entryupdate_4value,
        entrydelete_1namesize,
        entrydelete_2name
    };

    bool process_once();
    bool redirect_by_type();

    inline void back_to_start() {
        m_read_state = read_state::start;
        m_error_state = error_state::no_error;
    }

    inline void on_error(error_state error) {
        m_read_state = read_state::error;
        m_error_state = error;
    }

    obsr::io::buffer m_read_buffer;

    read_state m_read_state;
    error_state m_error_state;

    state m_state;
};*/

class client_reader {
public:
    struct state {
        static constexpr size_t message_buffer_size = 1024;
        message_header header;
        uint8_t message_buffer[message_buffer_size];
    };

    client_reader();

    bool has_full_result() const;
    bool has_error() const;
    const state& current_state() const;

    void reset();
    void update(obsr::os::readable* readable);
    void process();

private:
    enum class read_state {
        header = 1,
        message = 2,
        error = 15,
        start = header,
        end = 16,
    };
    enum class error_state {
        no_error,
        unsupported_size,
        unknown_state,
        read_failed
    };

    bool process_once();

    inline bool move_to_state(read_state state) {
        m_read_state = state;
        m_error_state = error_state::no_error;
        return true;
    }

    inline bool finished() {
        return move_to_state(read_state::end);;
    }

    inline bool try_later() {
        return false;
    }

    inline bool on_error(error_state error) {
        m_read_state = read_state::error;
        m_error_state = error;

        return false;
    }

    obsr::io::buffer m_read_buffer;
    read_state m_read_state;
    error_state m_error_state;
    state m_state;
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

    bool process();
    bool write(uint8_t type, uint8_t* buffer, size_t size);

private:
    void on_ready_resource(uint32_t flags);
    void on_read_ready();
    void on_write_ready();

    std::shared_ptr<obsr::io::nio_runner> m_nio_runner;
    obsr::handle m_resource_handle;

    std::shared_ptr<obsr::os::socket> m_socket;
    std::mutex m_mutex; // todo: seperate mutex for read/write maybe

    client_reader m_reader;
    obsr::io::buffer m_write_buffer;
    bool m_connecting;
    bool m_closed;

    listener* m_listener;
};

/*class running_client {
public:
    running_client(std::shared_ptr<nio_runner> nio_runner);

    void process();

private:
    enum class state {
        idle, // doing nothing
        opening, // opening and configuring the socket (client only)
        opened_need_config, // server start called, need initial config (server only)
        connecting, // connecting socket to remote (client only)
        connected, // connected and ready to operate

        waiting_for_handshake, // waiting for remote to send handshake (client: after connect)
        waiting_for_go, // waiting for final connection go ahead (client: after initial handshake sent)
        sent_initial_handshake, // sent a handshake message and waiting response (server: after sending handshake initial, waiting response)
    };

    bool process_once();

    bool on_open_socket(bool connect);

    std::shared_ptr<nio_runner> m_nio_runner;

    client_io m_io;
    connection_info m_info;

    std::mutex m_mutex;
    state m_current_state;
};*/

/*class client {
public:
    client(std::shared_ptr<nio_runner>& nio_runner);
    ~client();

    void start(connection_info info);
    void start(std::shared_ptr<obsr::os::socket>& socket);

    void process();

private:


    void write_handshake();
    void write(message_type type, uint8_t* buffer, size_t size);

    bool process_once();

    void on_ready_resource(uint32_t flags);
    void on_read_ready();
    void on_write_ready();

    bool on_open_socket();
    void free_resource();

    std::shared_ptr<nio_runner> m_nio_runner;

    std::shared_ptr<obsr::os::socket> m_socket;
    connection_info m_info;
    obsr::handle m_resource_handle;

    client_reader m_reader;
    bool m_read_ready;
    obsr::io::buffer m_write_buffer;

    std::mutex m_mutex;
    state m_current_state;
};*/

}
