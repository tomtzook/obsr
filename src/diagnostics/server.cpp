
#include <ranges>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <crow.h>

#include "server.h"

namespace obsr::diagnostics {

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

server::server(std::shared_ptr<storage::storage> storage)
    : m_storage_monitor(std::move(storage))
    , m_app()
    , m_thread(&server::server_main, this)
{}

server::~server() {
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
            writer.String(entry.path.c_str());
            writer.Key("value");
            write_value(writer, entry.value);

            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();

        return crow::response(200, "application/json", buffer.GetString());
    });

    m_app.port(18080).multithreaded().run();
}

}
