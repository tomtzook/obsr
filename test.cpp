
#include <obsr.h>


int main() {
    auto root = obsr::get_root();
    auto table = obsr::get_child(root, "test");
    auto entry_1 = obsr::get_entry(table, "hello");
    auto entry_2 = obsr::get_entry(table, "world");

    auto listener1 = obsr::listen_entry(entry_1, [](const obsr::event& event) -> void {
        printf("[1] Event called: type=0x%x, path=%s\n",
               static_cast<uint32_t>(event.type),
               event.path.c_str());
    });
    auto listener2 = obsr::listen_object(table, [](const obsr::event& event) -> void {
        printf("[2] Event called: type=0x%x, path=%s\n",
               static_cast<uint32_t>(event.type),
               event.path.c_str());
    });

    obsr::value_t value{};
    value.type = obsr::value_type::boolean;
    value.value.boolean = true;
    obsr::set_value(entry_1, value);

    obsr::get_value(entry_1, value);
    printf("%d\n", value.value.boolean);
    printf("entry_1: 0x%x\n", obsr::probe(entry_1));
    printf("entry_2: 0x%x\n", obsr::probe(entry_2));

    obsr::delete_object(table);

    printf("entry_1: 0x%x\n", obsr::probe(entry_1));
    printf("entry_2: 0x%x\n", obsr::probe(entry_2));

    obsr::delete_listener(listener1);
    obsr::delete_listener(listener2);

    return 0;
}
