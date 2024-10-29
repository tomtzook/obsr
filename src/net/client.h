#pragma once

#include <looper.h>
#include <looper_tcp.h>

#include "storage/storage.h"
#include "net/io.h"
#include "net/serialize.h"
#include "net/net.h"
#include "util/time.h"

namespace obsr::net::client {

class network_client : public network_interface {
public:
    explicit network_client(clock_ref& clock);

    void configure_target(connection_info info);

    void attach_storage(std::shared_ptr<storage::storage> storage) override;
    void start(looper::loop loop) override;
    void stop() override;

private:
    enum class state {
        idle,
        opening,
        connecting,
        in_handshake_time_sync,
        in_handshake,
        in_use
    };

    void update();
    bool do_open_and_connect();
    void process_storage();
    void process_new_data();
    void on_new_message(const message_header& header, const uint8_t* buffer, size_t size);
    bool write_new_message(uint8_t type, const uint8_t* buffer, size_t size);
    void close_io();

    std::mutex m_mutex;
    state m_state;

    clock_ref m_clock;
    std::shared_ptr<storage::storage> m_storage;
    connection_info m_conn_info;

    looper::loop m_loop;
    looper::tcp m_tcp;
    looper::timer m_update_timer_handle;

    reader m_reader;
    message_parser m_parser;
    message_queue m_message_queue;
    bool m_writing_in_progress;
    io::linear_buffer m_write_buffer;

    timer m_connect_retry_timer;
    timer m_clock_sync_timer;
};

}
