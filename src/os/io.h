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
    class poll_resource {
    public:
        virtual ~poll_resource() = default;

        virtual bool valid() const = 0;

        virtual bool has_result() const = 0;
        virtual uint32_t result_flags() const = 0;

        virtual uint32_t flags() const = 0;
        virtual void flags(uint32_t flags) = 0;
    };

    selector();
    ~selector();

    poll_resource* add(std::shared_ptr<resource> resource, uint32_t flags);
    void remove(poll_resource* resource);

    void poll(std::chrono::milliseconds timeout);

private:
    ssize_t find_empty_resource_index();
    void initialize_native_data();

    void* m_native_data;
    poll_resource* m_resources[max_resources];
};

}
