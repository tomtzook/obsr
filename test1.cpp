
#include <cstring>
#include <unistd.h>

#include "src/debug.h"
#include "src/util/time.h"
#include "obsr.h"

#include "src/events/events.h"
#include "src/os/poller.h"
#include "src/os/socket.h"
#include "src/net/io.h"


static void wait(std::chrono::milliseconds time) {
    auto start = obsr::time_now();
    auto now = start;
    while (now - start < time) {
        sleep(1);
        now = obsr::time_now();
    }
}

int main() {
    auto looper = std::make_shared<obsr::events::looper>(std::make_unique<obsr::os::resource_poller>());
    obsr::events::looper_thread looper_thread(looper);

    obsr::net::server_io server_io;
    obsr::net::socket_io socket_io;

    server_io.on_message([&server_io](obsr::net::server_io::client_id id, const obsr::net::message_header& header, const uint8_t* buffer, size_t size)->void {
        printf("[server] new message: id=%d, type=%d, size=%d\n", id, header.type, size);
    });
    server_io.on_connect([&server_io](obsr::net::server_io::client_id id)->void {
        printf("[server] client connected: id=%d\n", id);
        const char* msg = "hello";
        server_io.write_to(id, 1, reinterpret_cast<const uint8_t*>(msg), strlen(msg));
    });
    server_io.on_disconnect([&server_io](obsr::net::server_io::client_id id)->void {
        printf("[server] client disconnected: id=%d\n", id);
    });
    server_io.on_close([&server_io]()->void {
        printf("[server] closed\n");
    });

    socket_io.on_message([&socket_io](const obsr::net::message_header& header, const uint8_t* buffer, size_t size)->void {
        printf("[client] new message: type=%d, size=%d\n", header.type, size);
        const char* msg = "hello2";
        socket_io.write(1, reinterpret_cast<const uint8_t*>(msg), strlen(msg));
    });
    socket_io.on_connect([&socket_io]()->void {
        printf("[client] connected\n");
    });
    socket_io.on_close([&socket_io]()->void {
        printf("[client] close\n");
    });

    looper->request_execute([&server_io](obsr::events::looper& looper)->void {
        server_io.start(&looper, 5001);
    });
    looper->request_execute([&socket_io](obsr::events::looper& looper)->void {
        socket_io.start(&looper);
        socket_io.connect({"127.0.0.1", 5001});
    });

    wait(std::chrono::milliseconds(20000));

    looper->request_execute([&server_io, &socket_io](obsr::events::looper& looper)->void {
        socket_io.stop();
        server_io.stop();
    });

    wait(std::chrono::milliseconds(500));

    return 0;
}

