#pragma once

#include <thread>
#include <crow.h>

#include "diagnostics/storage.h"
#include "diagnostics/dispatcher.h"
#include "diagnostics/network.h"

namespace obsr::diagnostics {

class server {
public:
    explicit server(std::shared_ptr<storage::storage> storage, event_dispatcher_ptr dispatcher, uint16_t port);
    ~server();

private:
    void server_main();

    std::shared_ptr<storage::storage> m_storage;
    event_dispatcher_ptr m_dispatcher;
    storage_monitor m_storage_monitor;
    network_monitor m_network_monitor;
    crow::SimpleApp m_app;
    const uint16_t m_port;
    std::thread m_thread;
};

}
