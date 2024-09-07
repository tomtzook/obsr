
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
    auto entry2 = obsr::get_entry(table1, "send_time");
    auto entry3 = obsr::get_entry(table1, "arr");

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
                    case obsr::value_type::integer32_array: {
                        auto arr = value.get_int32_array();
                        for (int32_t i : arr) {
                            printf("%d, ", i);
                        }
                        printf("\n");
                        break;
                    }
                    default:
                        printf("other\n");
                        break;
                }
                break;
            }
        }
    });

    obsr::set_value(entry1, obsr::value::make_float(0.1));
    obsr::set_value(entry2, obsr::value::make_int64(0));

    auto start = obsr::time_now();
    auto now = start;
    while (now - start < std::chrono::milliseconds(20000)) {
        auto value = obsr::value::make_int64(now.count());
        obsr::set_value(entry2, value);

        int32_t a[] = {1, 2, 3, 4};
        value = obsr::value::make_int32_array(a);
        obsr::set_value(entry3, value);

        //sleep(1);
        usleep(20000);
        now = obsr::time_now();
    }

    return 0;
}

