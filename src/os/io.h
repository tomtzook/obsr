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

class selector {
public:
    static constexpr size_t max_resources = 10;
    enum poll_type {
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
        } m_states[max_resources];

        friend selector;
    };

    selector();
    ~selector();

    handle add(std::shared_ptr<resource> resource, uint32_t flags);
    void poll(poll_result& result, std::chrono::milliseconds timeout);

private:
    struct resource_data {
        explicit resource_data(std::shared_ptr<resource>& resource)
            : r_resource(resource)
        {}

        std::shared_ptr<resource> r_resource;
    };

    handle_table<resource_data, max_resources> m_handles;
    size_t m_resource_count;
    void* m_native_data;
};

}
