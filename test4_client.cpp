
#include <cstring>
#include <unistd.h>

#include "src/debug.h"
#include "src/util/time.h"
#include "obsr.h"

int main() {
    obsr::start_client("127.0.0.1", 50001);

    auto root = obsr::get_root();
    auto table1 = obsr::get_child(root, "hello");
    auto entry2 = obsr::get_entry(table1, "time");

    auto listener = obsr::listen_object(root, [](const obsr::event& event)->void {
        printf("EVENT notification: type=%d, path=%s\n",
               static_cast<uint8_t>(event.type),
               event.path.c_str());
    });

    auto start = obsr::time_now();
    auto now = start;
    while (now - start < std::chrono::milliseconds(10000)) {
        obsr::set_value(entry2, obsr::make_int64(now.count()));
        sleep(1);

        obsr::value_t value{};
        obsr::get_value(entry2, value);
        if (value.type != obsr::value_type::empty) {
            printf("Val: %ld\n", value.value.integer64);
        }
    }

    return 0;
}

