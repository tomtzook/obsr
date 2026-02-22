#pragma once

#include <thread>
#include <crow.h>

#include "storage.h"

namespace obsr::diagnostics {

class server {
public:
    explicit server(std::shared_ptr<storage::storage> storage);
    ~server();

private:
    void server_main();

    storage_monitor m_storage_monitor;
    crow::SimpleApp m_app;
    std::thread m_thread;
};

}
