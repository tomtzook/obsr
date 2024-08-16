
#include <thread>
#include <unistd.h>
#include <cstring>

#include "src/os/io.h"
#include "src/os/socket.h"
#include "src/internal_except.h"
#include "src/io/buffer.h"
#include "src/io/serialize.h"
#include "src/net/client.h"
#include "src/io/nio.h"
#include "src/debug.h"

struct context {
    std::shared_ptr<obsr::io::nio_runner> nio_runner;
    std::shared_ptr<obsr::os::server_socket> server_socket;
    std::shared_ptr<obsr::os::socket> client_socket;
};

#define LOG_MODULE "main"

class client_listener : public obsr::net::client_io::listener {
public:
    int i;
    obsr::net::client_io* client = nullptr;

    client_listener(int i) : i(i) {}

    void on_new_message(const obsr::net::message_header& header, const uint8_t* buffer, size_t size) override {
        TRACE_DEBUG(LOG_MODULE, "[%d] new message received, type=%d, size=%lu", i, header.type, size);

        if (client != nullptr && header.type != 3) {
            client->write(3, (uint8_t*) "ping", sizeof("ping"));
        }
    }
    void on_connected() override {
        TRACE_DEBUG(LOG_MODULE, "[%d] on connected", i);
    }
    void on_close() override {
        TRACE_DEBUG(LOG_MODULE, "[%d] on closed", i);
    }
};

void server_thread(context* context) {
    try {
        auto server_socket = context->server_socket;
        server_socket->setoption<obsr::os::sockopt_reuseport>(true);
        server_socket->bind("0.0.0.0", 50001);
        server_socket->listen(2);
        auto socket_u = server_socket->accept();

        std::shared_ptr<obsr::os::socket> socket{std::move(socket_u)};

        TRACE_DEBUG(LOG_MODULE, "[%d] new socket", 1);

        client_listener listener(1);
        obsr::net::client_io io(context->nio_runner, &listener);
        listener.client = &io;
        io.start(socket);

        while (!io.is_closed()) {
            sleep(1);
        }

        sleep(5);
    } catch (obsr::io_exception& e) {
        TRACE_DEBUG(LOG_MODULE, "[%d]: exception: %d, %s", 1, e.get_code(), strerror(e.get_code()));

    }
}

void client_thread(context* context) {
    sleep(5);

    try {
        auto socket = context->client_socket;
        client_listener listener(2);
        obsr::net::client_io io(context->nio_runner, &listener);
        listener.client = &io;
        io.start(socket);
        io.connect({"127.0.0.1", 50001});

        TRACE_DEBUG(LOG_MODULE, "[%d] writing to socket", 2);
        io.write(0, (uint8_t*) "hello", sizeof("hello"));
        io.write(1, (uint8_t*) "try", sizeof("try"));

        while (!io.is_closed()) {
            sleep(1);
        }

    } catch (obsr::io_exception& e) {
        TRACE_DEBUG(LOG_MODULE, "[%d]: exception: %d, %s", 2, e.get_code(), strerror(e.get_code()));
    }
}

int main() {
    auto nio_runner = std::make_shared<obsr::io::nio_runner>();
    auto server_socket = std::make_shared<obsr::os::server_socket>();
    auto socket = std::make_shared<obsr::os::socket>();

    context context = {nio_runner, server_socket, socket};

    std::thread thread1(server_thread, &context);
    std::thread thread2(client_thread, &context);

    thread1.join();
    thread2.join();

    return 0;
}

