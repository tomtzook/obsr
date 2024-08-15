
#include <thread>
#include <unistd.h>
#include <cstring>

#include "src/os/io.h"
#include "src/os/socket.h"
#include "src/internal_except.h"
#include "src/io/buffer.h"
#include "src/io/serialize.h"
#include "src/net/client.h"
#include "src/net/nio.h"

struct context {
    std::shared_ptr<obsr::net::nio_runner> nio_runner;
    std::shared_ptr<obsr::os::server_socket> server_socket;
    std::shared_ptr<obsr::os::socket> client_socket;
};

class client_listener : public obsr::net::client_io::listener {
public:
    void on_new_message(const obsr::net::message_header& header, const uint8_t* buffer, size_t size) override {
        printf("new message received, type=%d, size=%lu \n",
               header.type, size);
    }
    void on_connected() override {
        printf("on connected\n");
    }
    void on_close() override {
        printf("on closed\n");
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

        printf("[1] new socket\n");

        client_listener listener;
        obsr::net::client_io io(context->nio_runner, &listener);
        io.start(socket);

        while (io.process()) {
            sleep(1);
        }

        sleep(5);
    } catch (obsr::io_exception& e) {
        printf("[1]: exception: %d, %s\n", e.get_code(), strerror(e.get_code()));
    }
}

void client_thread(context* context) {
    sleep(5);

    try {
        auto socket = context->client_socket;
        client_listener listener;
        obsr::net::client_io io(context->nio_runner, &listener);
        io.start(socket);
        io.connect({"127.0.0.1", 50001});

        printf("[2] wrote to socket\n");
        io.write(0, (uint8_t*) "hello", sizeof("hello"));

        while (io.process()) {
            sleep(1);
        }

    } catch (obsr::io_exception& e) {
        printf("[2]: exception: %d, %s\n", e.get_code(), strerror(e.get_code()));
    }
}

int main() {
    auto nio_runner = std::make_shared<obsr::net::nio_runner>();
    auto server_socket = std::make_shared<obsr::os::server_socket>();
    auto socket = std::make_shared<obsr::os::socket>();

    context context = {nio_runner, server_socket, socket};

    std::thread thread1(server_thread, &context);
    std::thread thread2(client_thread, &context);

    thread1.join();
    thread2.join();

    return 0;
}

