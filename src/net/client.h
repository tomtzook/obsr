#pragma once

#include "storage/storage.h"
#include "net/io.h"
#include "net/serialize.h"

namespace obsr::net {

// todo: need base server to be shared by
//  actual clients for server processes
//  server clients in server processes
// todo: consider how to initialize new entry from name to id
// todo: how to do handshake where server tells us of existing entries?
// todo: comm-storage data transfer
// todo: need some keep alive stuff?
// todo: extract what we can to share with server code
// todo: work on serialize info, need linear buffer
// todo: network should sit on the storage instead of linked with interfaces
//      allow client to mark stuff on the entries and iterate over them
//      isn't straight forward because we need some thread safety as well as to make sure we don't starve users
//      we also don't want the client to have a complete copy of data

// todo: this how its done
//      net stuff use storage directly (via specialized functions)
//      periodically, query storage for changes via a queue pointing at entries
//          when iterating update during iteration on what's going on
//      problem: must attach storage to client before use!
//      must store net related stuff in storage

class client : public socket_io::listener {
public:
    client(std::shared_ptr<io::nio_runner> nio_runner);

    void attach_storage(std::shared_ptr<storage::storage> storage);

    void start(connection_info info);
    void stop();

    void process();

    // events
    void on_new_message(const message_header& header, const uint8_t* buffer, size_t size) override;
    void on_connected() override;
    void on_close() override;

private:
    enum class state {
        idle,
        opening,
        connecting,
        in_handshake,
        in_use
    };

    bool open_socket_and_start_connection();

    bool write_entry_created(const storage::storage_entry& entry);
    bool write_entry_updated(const storage::storage_entry& entry);
    bool write_entry_deleted(const storage::storage_entry& entry);

    state m_state;
    std::mutex m_mutex;

    socket_io m_io;
    connection_info m_conn_info;
    message_parser m_parser;
    message_writer m_writer;
    std::shared_ptr<storage::storage> m_storage;
};

}
