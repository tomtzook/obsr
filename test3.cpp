
#include <thread>
#include <unistd.h>
#include <cstring>

#include "src/os/io.h"
#include "src/os/socket.h"
#include "src/internal_except.h"
#include "src/io/buffer.h"
#include "src/io/serialize.h"
#include "src/net/io.h"
#include "src/io/nio.h"
#include "src/debug.h"
#include "src/updater.h"
#include "src/net/server.h"
#include "src/net/client.h"
#include "src/util/time.h"

#define LOG_MODULE "main"

int main() {
    auto nio_runner = std::make_shared<obsr::io::nio_runner>();
    auto updater = std::make_shared<obsr::updater>();
    auto listener_storage = std::make_shared<obsr::storage::listener_storage>();
    auto storage_server = std::make_shared<obsr::storage::storage>(listener_storage);
    auto storage_client = std::make_shared<obsr::storage::storage>(listener_storage);

    auto server = std::make_shared<obsr::net::server>(nio_runner);
    auto client = std::make_shared<obsr::net::client>(nio_runner);

    updater->attach(server, std::chrono::milliseconds(100));
    updater->attach(client, std::chrono::milliseconds(100));

    server->attach_storage(storage_server);
    client->attach_storage(storage_client);

    server->start(50001);

    auto entry_server = storage_server->get_or_create_entry("hello");
    storage_server->set_entry_value(entry_server, obsr::make_int64(100));
    auto entry_client = storage_client->get_or_create_entry("hello");
    auto client_listener = storage_client->listen("", [](const obsr::event& event)->void {
        printf("CLIENT event notification: type=%d, path=%s\n",
               static_cast<uint8_t>(event.type),
               event.path.c_str());
    });

    client->start({"127.0.0.1", 50001});

    auto start = obsr::time_now();
    auto now = start;
    while (now - start < std::chrono::milliseconds(10000)) {
        sleep(1);

        /*obsr::value value{};
        storage_client->get_entry_value(entry_client, value);
        if (value.type != obsr::value_type::empty) {
            printf("not empty!\n");
            printf("type: %d\n", static_cast<uint8_t>(value.type));
            printf("val: %ld\n", value.value.integer64);
            break;
        }*/
    }

    return 0;
}

