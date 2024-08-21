#pragma once

#include "storage/storage.h"
#include "net/io.h"
#include "net/serialize.h"
#include "net/net.h"
#include "util/time.h"

namespace obsr::net {

// todo: switch to using a message queue?
// todo: manage like a proper state machine

class client : public socket_io::listener, public network_interface {
public:
    explicit client(const std::shared_ptr<io::nio_runner>& nio_runner, const std::shared_ptr<clock>& clock);

    void attach_storage(std::shared_ptr<storage::storage> storage) override;

    void start(connection_info info);
    void stop() override;

    void update() override;

    // events
    void on_new_message(const message_header& header, const uint8_t* buffer, size_t size) override;
    void on_connected() override;
    void on_close() override;

private:
    enum class state {
        idle,
        opening,
        connecting,
        in_handshake_time_sync,
        in_handshake_time_sync_request_sent,
        in_handshake_signal_server_ready,
        in_handshake,
        in_use
    };

    bool open_socket_and_start_connection();

    bool write_entry_created(const storage::storage_entry& entry);
    bool write_entry_updated(const storage::storage_entry& entry);
    bool write_entry_deleted(const storage::storage_entry& entry);
    bool write_server_sync();
    bool write_handshake_ready();

    std::shared_ptr<clock> m_clock;

    state m_state;
    std::mutex m_mutex;

    socket_io m_io;
    connection_info m_conn_info;
    message_parser m_parser;
    message_serializer m_serializer;
    std::shared_ptr<storage::storage> m_storage;

    timer m_connect_retry_timer;
    timer m_clock_sync_timer;
    clock::sync_data m_clock_sync_data;
};

}
