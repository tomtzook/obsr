
#include <ranges>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <crow.h>

#include "server.h"

namespace obsr::diagnostics {

template<typename t>
std::string mask_str(const t mask, const char*(*bit_to_str)(t)) {
    std::stringstream ss;
    bool has_one = false;
    for (int i = 0; i < sizeof(mask) * 8; i++) {
        if (const auto bit = mask & (1 << i)) {
            const auto str = bit_to_str(bit);

            if (has_one) {
                ss << '|';
            } else {
                has_one = true;
            }

            ss << str;
        }
    }

    return ss.str();
}

static const char* flag_str(const uint16_t flag) {
    switch (flag) {
        case obsr::storage::flag_internal_dirty:
            return "dirty";
        case obsr::storage::flag_internal_created:
            return "created";
        case obsr::storage::flag_internal_deleted:
            return "deleted";
        default:
            return "";
    }
}

static const char* direction_to_str(const network_message::data_direction direction) {
    switch (direction) {
        case network_message::data_direction::in:
            return "in";
        case network_message::data_direction::out:
            return "out";
        default:
            return "";
    }
}

static const char* message_type_str(const net::message_type type) {
    switch (type) {
        case net::message_type::no_type:
            return "no_type";
        case net::message_type::entry_create:
            return "entry_create";
        case net::message_type::entry_update:
            return "entry_update";
        case net::message_type::entry_delete:
            return "entry_delete";
        case net::message_type::entry_id_assign:
            return "entry_id_assign";
        case net::message_type::handshake_finished:
            return "handshake_finished";
        case net::message_type::handshake_ready:
            return "handshake_ready";
        case net::message_type::time_sync_request:
            return "time_sync_request";
        case net::message_type::time_sync_response:
            return "time_sync_response";
        default:
            return "";
    }
}

template<typename wr_t_>
static void write_flags(rapidjson::Writer<wr_t_>& writer, const uint16_t flags) {
    writer.StartArray();

    for (int i = 0; i < sizeof(flags) * 8; i++) {
        if (const auto bit = flags & (1 << i)) {
            const auto str = flag_str(bit);
            writer.String(str);
        }
    }

    writer.EndArray();
}

template<typename wr_t_, typename t_>
static void write_arr(rapidjson::Writer<wr_t_>& writer, const std::span<const t_> arr) {
    writer.StartArray();

    for (const auto& item : arr) {
        if constexpr (std::is_same_v<t_, int32_t>) {
            writer.Int(item);
        } else if constexpr (std::is_same_v<t_, int64_t>) {
            writer.Int64(item);
        } else if constexpr (std::is_same_v<t_, float> || std::is_same_v<t_, double>) {
            writer.Double(item);
        } else {
            static_assert(false, "type not supported");
        }
    }

    writer.EndArray();
}

template<typename wr_t_>
static void write_value(rapidjson::Writer<wr_t_>& writer, const value& value) {
    writer.StartObject();

    const auto type = value.get_type();
    writer.Key("type");
    writer.String(value_type_str(type));

    writer.Key("value");
    switch (type) {
        case value_type::raw:
            break;
        case value_type::string:
            writer.String(value.get_string().data());
            break;
        case value_type::boolean:
            writer.Bool(value.get_boolean());
            break;
        case value_type::integer32:
            writer.Int(value.get_int32());
            break;
        case value_type::integer64:
            writer.Int64(value.get_int64());
            break;
        case value_type::floating_point32:
            writer.Double(value.get_float());
            break;
        case value_type::floating_point64:
            writer.Double(value.get_double());
            break;
        case value_type::integer32_array:
            write_arr(writer, value.get_int32_array());
            break;
        case value_type::integer64_array:
            write_arr(writer, value.get_int64_array());
            break;
        case value_type::floating_point32_array:
            write_arr(writer, value.get_float_array());
            break;
        case value_type::floating_point64_array:
            write_arr(writer, value.get_double_array());
            break;
        case value_type::empty:
        default:
            writer.Null();
            break;
    }

    writer.EndObject();
}

server::server(std::shared_ptr<storage::storage> storage, event_dispatcher_ptr dispatcher, const uint16_t port)
    : m_storage(std::move(storage))
    , m_dispatcher(std::move(dispatcher))
    , m_storage_monitor(m_storage, m_dispatcher)
    , m_network_monitor(m_dispatcher)
    , m_app()
    , m_port(port)
    , m_thread(&server::server_main, this) {
}

server::~server() {
    m_app.stop();
    m_thread.join();
}

void server::server_main() {
    m_storage_monitor.start();
    m_network_monitor.start();

    CROW_ROUTE(m_app, "/api/storage")([this](){
        m_storage_monitor.sync(); // todo: better place

        const auto data = m_storage_monitor.get_data_snapshot();

        rapidjson::StringBuffer buffer;
        rapidjson::Writer writer(buffer);

        writer.StartObject();
        writer.Key("entries");
        writer.StartArray();
        for (const auto& entry : *data | std::ranges::views::values) {
            writer.StartObject();

            writer.Key("path");
            writer.String(entry.path);
            writer.Key("net_id");
            writer.Uint(entry.net_id);
            writer.Key("flags");
            write_flags(writer, entry.flags);
            writer.Key("value");
            write_value(writer, entry.value);

            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();

        return crow::response(200, "application/json", buffer.GetString());
    });

    CROW_ROUTE(m_app, "/api/net/history")([this](const crow::request& req) {
        const auto client_id_str = req.url_params.get("client_id");
        const auto message_id_str = req.url_params.get("message_id");
        const auto direction_str = req.url_params.get("direction");
        const auto limit_str = req.url_params.get("limit");
        if (!limit_str) {
            return crow::response(400);
        }

        const auto client_id_int = client_id_str != nullptr ? std::stoi(client_id_str) : -1;
        const auto message_id_int = message_id_str != nullptr ? std::stoi(message_id_str) : -1;
        const auto direction_int = direction_str != nullptr ? std::stoi(direction_str) : 0;
        const auto direction = direction_int <= 1 ? std::optional{static_cast<network_message::data_direction>(direction_int)} : std::nullopt;
        const auto limit = std::stoi(limit_str);

        const auto data = m_network_monitor.get_data_snapshot(limit,
            [client_id_int, message_id_int, &direction](const auto& msg)->bool {
                if (client_id_int > 0 && client_id_int != msg.client_id && msg.client_id != all_client_id) {
                    return false;
                }
                if (message_id_int > 0 && message_id_int != msg.message_id) {
                    return false;
                }
                if (direction && msg.direction != direction.value()) {
                    return false;
                }


                return true;
        });

        rapidjson::StringBuffer buffer;
        rapidjson::Writer writer(buffer);

        writer.StartObject();
        writer.Key("messages");
        writer.StartArray();
        for (const auto& entry : data) {
            writer.StartObject();

            writer.Key("direction");
            writer.String(direction_to_str(entry.direction));
            writer.Key("type");
            writer.String(message_type_str(entry.type));
            writer.Key("client_id");
            writer.Uint(entry.client_id);
            writer.Key("message_id");
            writer.Uint64(entry.message_id);
            writer.Key("timestamp");
            writer.Int64(entry.timestamp.count());

            switch (entry.type) {
                case net::message_type::entry_create:
                    writer.Key("entry_id");
                    writer.Uint(entry.data.entry_create.id);
                    break;
                case net::message_type::entry_update:
                    writer.Key("entry_id");
                    writer.Uint(entry.data.entry_update.id);
                    break;
                case net::message_type::entry_delete:
                    writer.Key("entry_id");
                    writer.Uint(entry.data.entry_delete.id);
                    break;
                case net::message_type::entry_id_assign:
                    writer.Key("entry_id");
                    writer.Uint(entry.data.entry_id_assign.id);
                    break;
                case net::message_type::handshake_finished:
                case net::message_type::handshake_ready:
                case net::message_type::time_sync_request:
                case net::message_type::time_sync_response:
                case net::message_type::no_type:
                    break;
            }

            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();

        return crow::response(200, "application/json", buffer.GetString());
    });

    CROW_ROUTE(m_app, "/api/net/connections")([this](const crow::request& req) {
        const auto data = m_network_monitor.get_connections();

        rapidjson::StringBuffer buffer;
        rapidjson::Writer writer(buffer);

        writer.StartObject();
        writer.Key("messages");
        writer.StartArray();
        for (const auto& [id, addr] : data) {
            writer.StartObject();

            writer.Key("id");
            writer.Uint(id);
            writer.Key("address");
            writer.String(addr.ip.c_str());
            writer.Key("port");
            writer.Uint(addr.port);

            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();

        return crow::response(200, "application/json", buffer.GetString());
    });

    m_app.port(m_port).multithreaded().run();
}

}
