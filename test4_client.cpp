
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
        switch (event.type) {
            case obsr::event_type::created:
                printf("\t path created\n");
                break;
            case obsr::event_type::deleted:
                printf("\t path deleted\n");
                break;
            case obsr::event_type::value_changed:
                printf("\t value changed, val=%ld\n", event.value.value.integer64);
                break;
        }
    });

    auto start = obsr::time_now();
    auto now = start;
    while (now - start < std::chrono::milliseconds(30000)) {
        sleep(1);
        now = obsr::time_now();
    }

    return 0;
}

