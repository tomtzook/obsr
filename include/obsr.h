#pragma once

#include <string>
#include <functional>
#include <chrono>
#include <ostream>

#include "obsr_types.h"

namespace obsr {

std::chrono::milliseconds time();

object get_root();
object get_child(object obj, std::string_view name);
entry get_entry(object obj, std::string_view name);

void delete_object(object obj);
void delete_entry(entry entry);

uint32_t probe(entry entry);
obsr::value get_value(entry entry);
void set_value(entry entry, const obsr::value& value);
void clear_value(entry entry);

listener listen_object(object obj, const listener_callback&& callback);
listener listen_entry(entry entry, const listener_callback&& callback);
void delete_listener(listener listener);

void start_server(uint16_t bind_port);
void start_client(std::string_view address, uint16_t server_port);
void stop_network();

}

std::ostream& operator<<(std::ostream& os, obsr::value_type type);
std::ostream& operator<<(std::ostream& os, const obsr::value& value);
