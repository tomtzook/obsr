
#include <thread>
#include <unistd.h>
#include <cstring>

#include "obsr.h"
#include "src/util/time.h"

int main() {
    obsr::start_server(50001);

    auto root = obsr::get_root();
    auto table1 = obsr::get_child(root, "hello");
    auto entry1 = obsr::get_entry(table1, "11");
    auto entry2 = obsr::get_entry(table1, "time");

    auto listener = obsr::listen_object(root, [](const obsr::event& event)->void {
        printf("EVENT notification: type=%d, path=%s\n",
               static_cast<uint8_t>(event.type),
               event.path.c_str());
    });

    obsr::set_value(entry1, obsr::make_float(0.1));
    obsr::set_value(entry2, obsr::make_int64(0));

    auto start = obsr::time_now();
    auto now = start;
    while (now - start < std::chrono::milliseconds(10000)) {
        auto value = obsr::make_int64(now.count());
        printf("Val, write: %ld\n", value.value.integer64);
        obsr::set_value(entry2, value);

        sleep(1);
        now = obsr::time_now();
    }

    return 0;
}

