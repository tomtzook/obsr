
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

server::server(std::shared_ptr<storage::storage> storage, const uint16_t port)
    : m_storage(std::move(storage))
    , m_dispatcher(std::make_shared<event_dispatcher>())
    , m_storage_monitor(m_storage, m_dispatcher)
    , m_app()
    , m_port(port)
    , m_thread(&server::server_main, this) {
    m_storage->set_diagnostics_dispatcher(m_dispatcher);
}

server::~server() {
    m_storage->set_diagnostics_dispatcher(m_dispatcher);

    m_app.stop();
    m_thread.join();
}

void server::server_main() {
    m_storage_monitor.start();

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
            writer.Key("netId");
            writer.Int(entry.net_id);
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

    m_app.port(m_port).multithreaded().run();
}

}
