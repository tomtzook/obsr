
#include <utility>

#include "debug.h"
#include "internal_except.h"
#include "util/general.h"
#include "client.h"

namespace obsr::net {

#define LOG_MODULE "client"

client::client(std::shared_ptr<io::nio_runner> nio_runner)
    : m_state(state::idle)
    , m_mutex()
    , m_io(std::move(nio_runner), this)
    , m_conn_info()
    , m_parser()
    , m_storage()
{}

void client::attach_storage(std::shared_ptr<storage::storage> storage) {
    m_storage = std::move(storage);
}

void client::start(connection_info info) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::idle) {
        throw illegal_state_exception();
    }

    if (!m_storage) {
        throw illegal_state_exception();
    }

    m_conn_info = std::move(info);
    m_state = state::opening;
}

void client::stop() {
    std::unique_lock lock(m_mutex);

    if (m_state == state::idle) {
        throw illegal_state_exception();
    }
}

void client::process() {
    std::unique_lock lock(m_mutex);

    // todo: add sending keep alive?

    switch (m_state) {
        case state::opening: {
            m_storage->clear_net_ids();

            // todo: we may want a delay before trying again
            //  our caller is delayed, but we may want more
            if (open_socket_and_start_connection()) {
                // successful
                m_state = state::connecting;
            }

            break;
        }
        case state::in_use: {
            m_storage->act_on_dirty_entries([this](const storage::storage_entry& entry)->bool {
                // todo: may need to use timestamps to properly check if we already sent about this or not

                bool continue_run = false;
                if (entry.has_flags(storage::flag_internal_deleted)) {
                    // entry was deleted

                    if (entry.get_net_id() == storage::id_not_assigned) {
                        continue_run = true;
                    } else {
                        continue_run = write_entry_deleted(entry);
                    }
                } else {
                    // entry value was updated
                    if (entry.get_net_id() == storage::id_not_assigned) {
                        continue_run = write_entry_created(entry);
                    } else {
                        continue_run = write_entry_updated(entry);
                    }
                }

                // we want to mark un-dirty and resume if we succeeded
                return continue_run;
            });

            // todo: iterate over dirty entries and send
            break;
        }

        case state::in_handshake:
            // in this phase we do not send anything to the server, just receive data
        case state::connecting:
        case state::idle:
        default:
            break;
    }
}

void client::on_new_message(const message_header& header, const uint8_t* buffer, size_t size) {
    std::unique_lock lock(m_mutex);

    auto type = static_cast<message_type>(header.type);
    m_parser.set_data(type, buffer, size);
    m_parser.process();

    if (m_parser.is_errored()) {
        TRACE_DEBUG(LOG_MODULE, "failed to parse incoming data, parser error=%d", m_parser.error_code());
        return;
    } else if (!m_parser.is_finished()) {
        TRACE_DEBUG(LOG_MODULE, "failed to parse incoming data, parser did not finish");
        return;
    }

    auto parse_data = m_parser.data();
    switch (type) {
        case message_type::entry_create:
            invoke_shared_ptr<storage::storage, storage::entry_id, const std::string&, const value_t&>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_created,
                    parse_data.id,
                    parse_data.name,
                    parse_data.value);
            break;
        case message_type::entry_update:
            invoke_shared_ptr<storage::storage, storage::entry_id, const value_t&>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_updated,
                    parse_data.id,
                    parse_data.value);
            break;
        case message_type::entry_delete:
            invoke_shared_ptr<storage::storage, storage::entry_id>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_deleted,
                    parse_data.id);
            break;
        case message_type::entry_id_assign:
            invoke_shared_ptr<storage::storage, storage::entry_id, const std::string&>(
                    lock,
                    m_storage,
                    &storage::storage::on_entry_id_assigned,
                    parse_data.id,
                    parse_data.name);
            break;
    }
}

void client::on_connected() {
    std::unique_lock lock(m_mutex);

    m_state = state::in_handshake;
}

void client::on_close() {
    std::unique_lock lock(m_mutex);

    m_state = state::opening;
}

bool client::open_socket_and_start_connection() {
    try {
        auto socket = std::make_shared<obsr::os::socket>();
        socket->setoption<os::sockopt_reuseport>(true);

        m_io.start(socket);
        m_io.connect(m_conn_info);

        return true;
    } catch (...) {
        if (!m_io.is_stopped()) {
            m_io.stop();
        }

        return false;
    }
}

bool client::write_entry_created(const storage::storage_entry& entry) {
    m_writer.reset();

    value_t value{};
    entry.get_value(value);
    if (!m_writer.entry_created(entry.get_net_id(), entry.get_path(), value)) {
        return false;
    }

    if (!m_io.write(static_cast<uint8_t>(message_type::entry_create), m_writer.data(), m_writer.size())) {
        return false;
    }

    return true;
}

bool client::write_entry_updated(const storage::storage_entry& entry) {
    m_writer.reset();

    value_t value{};
    entry.get_value(value);
    if (!m_writer.entry_updated(entry.get_net_id(), value)) {
        return false;
    }

    if (!m_io.write(static_cast<uint8_t>(message_type::entry_update), m_writer.data(), m_writer.size())) {
        return false;
    }

    return true;
}

bool client::write_entry_deleted(const storage::storage_entry& entry) {
    m_writer.reset();

    if (!m_writer.entry_deleted(entry.get_net_id())) {
        return false;
    }

    if (!m_io.write(static_cast<uint8_t>(message_type::entry_delete), m_writer.data(), m_writer.size())) {
        return false;
    }

    return true;
}

}
