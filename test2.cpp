
#include <thread>
#include <unistd.h>
#include <cstring>

#include "src/os/io.h"
#include "src/os/socket.h"
#include "src/internal_except.h"

void server_thread() {
    obsr::os::server_socket server_socket;
    try {
        printf("setopt\n");
        server_socket.setoption<obsr::os::sockopt_reuseport>(true);
        printf("bind\n");
        server_socket.bind("0.0.0.0", 50001);
        printf("listen\n");
        server_socket.listen(2);

        printf("accept\n");
        auto socket = server_socket.accept();
        printf("server accepted socket\n");

        uint8_t buffer[1024];
        const auto read = socket->read(buffer, 1024);
        buffer[read + 1] = 0;
        printf("%s\n", buffer);

        memcpy(buffer, "hello from here", sizeof("hello from here"));
        socket->write(buffer, sizeof("hello from here"));
        sleep(5);
    } catch (obsr::io_exception& e) {
        printf("[1]: exception: %d, %s\n", e.get_code(), strerror(e.get_code()));
    }
}

void client_thread() {
    sleep(5);

    obsr::os::selector selector;
    auto socket = std::make_shared<obsr::os::socket>();
    obsr::handle handle = selector.add(socket, obsr::os::selector::poll_in);

    try {
        socket->connect("127.0.0.1", 50001);

        uint8_t buffer[] = "hey man";
        socket->write(buffer, sizeof(buffer));

        obsr::os::selector::poll_result poll_result{};
        selector.poll(poll_result, std::chrono::milliseconds(2000));
        if (poll_result.has(handle, obsr::os::selector::poll_in)) {
            printf("heyyyyyyyyyyyyyyyyyy\n");
        }
    } catch (obsr::io_exception& e) {
        printf("[2]: exception: %d, %s\n", e.get_code(), strerror(e.get_code()));
    }
}

int main() {
    std::thread thread1(server_thread);
    std::thread thread2(client_thread);

    thread1.join();
    thread2.join();

    return 0;
}

