#pragma once

#include <functional>
#include <unordered_map>
#include <memory>
#include <chrono>

#include "obsr_types.h"
#include "obsr_except.h"
#include "util/handles.h"

namespace obsr::os {

class selector;

class resource {
public:
    explicit resource(int fd);
    virtual ~resource();

    void close();

protected:
    [[nodiscard]] inline int fd() const {
        return m_fd;
    }

    void throw_if_closed() const;

private:
    int m_fd;

    friend class selector;
};

class readable {
public:
    virtual size_t read(uint8_t* buffer, size_t buffer_size) = 0;
};

class writable {
public:
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;
};

class selector {
public:
    static constexpr size_t max_resources = 10;
    enum poll_type : uint32_t {
        poll_in = (0x1 << 0),
        poll_in_urgent = (0x1 << 1),
        poll_out = (0x1 << 2),
        poll_error = (0x1 << 3),
        poll_hung = (0x1 << 4)
    };

    struct poll_result {
    public:
        poll_result();

        bool has(obsr::handle handle) const;
        bool has(obsr::handle handle, poll_type type) const;
        bool has(obsr::handle handle, uint32_t flags) const;

        uint32_t get(obsr::handle handle) const;

    private:
        struct {
            obsr::handle handle;
            uint32_t flags;
            std::chrono::milliseconds update_time;
        } m_states[max_resources];
        std::chrono::milliseconds m_last_update_time;

        friend selector;
    };

    selector();
    ~selector();

    handle add(std::shared_ptr<resource> resource, uint32_t flags);
    std::shared_ptr<resource> remove(obsr::handle handle);

    void poll(poll_result& result, std::chrono::milliseconds timeout);

private:
    struct resource_data {
        explicit resource_data(std::shared_ptr<resource>& resource)
            : r_resource(resource)
            , r_index(0)
        {}

        std::shared_ptr<resource> r_resource;
        size_t r_index;
    };

    ssize_t find_empty_resource_index();
    void initialize_native_data();

    handle_table<resource_data, max_resources> m_handles;
    void* m_native_data;
};

}
