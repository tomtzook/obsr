#pragma once

#include <functional>
#include <unordered_map>
#include <memory>
#include <chrono>

#include "obsr_types.h"
#include "obsr_except.h"
#include "util/handles.h"

namespace obsr::os {

using descriptor = int;

class resource {
public:
    explicit resource(descriptor resource_descriptor);
    virtual ~resource();

    void close();

    [[nodiscard]] inline descriptor get_descriptor() const {
        return m_descriptor;
    }

protected:
    void throw_if_closed() const;

private:
    descriptor m_descriptor;
};

class readable {
public:
    virtual size_t read(uint8_t* buffer, size_t buffer_size) = 0;
};

class writable {
public:
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;
};



}
