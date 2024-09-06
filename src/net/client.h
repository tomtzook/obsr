#pragma once

#include "storage/storage.h"
#include "net/io.h"
#include "net/serialize.h"
#include "net/net.h"
#include "util/time.h"
#include "events/events.h"

namespace obsr::net {

class network_client : public network_interface {
public:
    explicit network_client(std::shared_ptr<clock>& clock);

    void configure_target(connection_info info);

    void attach_storage(std::shared_ptr<storage::storage> storage) override;
    void start(events::looper* looper) override;
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

    std::mutex m_mutex;
    state m_state;

    std::shared_ptr<clock> m_clock;
    std::shared_ptr<storage::storage> m_storage;
    connection_info m_conn_info;

    events::looper* m_looper;
    obsr::handle m_update_timer_handle;

    socket_io m_io;
    message_parser m_parser;
    message_queue m_message_queue;

    timer m_connect_retry_timer;
    timer m_clock_sync_timer;
};

}
