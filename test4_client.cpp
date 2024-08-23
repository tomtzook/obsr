
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
        printf("EVENT notification: m_type=%d, path=%s\n",
               static_cast<uint8_t>(event.get_type()),
               event.get_path().c_str());
        switch (event.get_type()) {
            case obsr::event_type::created:
                printf("\t path created\n");
                break;
            case obsr::event_type::deleted:
                printf("\t path deleted\n");
                break;
            case obsr::event_type::value_changed: {
                const auto value = event.get_value();
                printf("\t value changed, m_type=%d val=", static_cast<uint8_t>(value.get_type()));
                switch (value.get_type()) {
                    case obsr::value_type::empty:
                        printf("empty\n");
                        break;
                    case obsr::value_type::integer64:
                        printf("%ld\n", value.get_int64());
                        break;
                    default:
                        printf("other\n");
                        break;
                }
                printf("\t now time=%ld\n", obsr::time().count());
                printf("\t now time actual=%ld\n", obsr::time_now().count());
                break;
            }
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

