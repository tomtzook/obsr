
#include <cstring>
#include <unistd.h>

#include "src/debug.h"
#include "src/util/time.h"
#include "obsr.h"

#include "src/events/events.h"
#include "src/os/poller.h"
#include "src/os/socket.h"
#include "src/net/io.h"


class server_listener : public obsr::net::server_io::listener {

    obsr::net::server_io& m_io;

    void on_client_connected(obsr::net::server_io::client_id id) override {
        printf("[server] network_client connected: id=%d\n", id);

        const char* msg = "hello";
        m_io.write_to(id, 1, reinterpret_cast<const uint8_t*>(msg), strlen(msg));
    }

    void on_client_disconnected(obsr::net::server_io::client_id id) override {
        printf("[server] network_client disconnected: id=%d\n", id);
    }

    void on_new_message(obsr::net::server_io::client_id id, const obsr::net::message_header& header, const uint8_t* buffer, size_t size) override {
        printf("[server] network_client new message: id=%d, type=%d, size=%d\n", id, header.type, size);
    }

    void on_close() override {
        printf("[server] close\n");
    }

public:
    server_listener(obsr::net::server_io& io)
        : m_io(io)
    {}
};

class client_listener : public obsr::net::socket_io::listener {

    obsr::net::socket_io& m_io;

    void on_new_message(const obsr::net::message_header& header, const uint8_t* buffer, size_t size) override {
        printf("[network_client] new message: type=%d, size=%d\n", header.type, size);
        const char* msg = "hello2";
        m_io.write(1, reinterpret_cast<const uint8_t*>(msg), strlen(msg));
    }

    void on_connected() override {
        printf("[network_client] connected\n");
    }

    void on_close() override {
        printf("[network_client] close\n");
    }

public:
    client_listener(obsr::net::socket_io& io)
        : m_io(io)
    {}
};

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

    server_listener server_listener(server_io);
    client_listener client_listener(socket_io);

    looper->request_execute([&server_io, &server_listener](obsr::events::looper& looper)->void {
        server_io.start(&looper, &server_listener, 5001);
    });
    looper->request_execute([&socket_io, &client_listener](obsr::events::looper& looper)->void {
        socket_io.start(&looper, &client_listener);
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

