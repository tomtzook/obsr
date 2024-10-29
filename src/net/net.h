#pragma once

#include <looper.h>

#include "storage/storage.h"

namespace obsr::net {

class network_interface {
public:
    virtual ~network_interface() = default;

    virtual void attach_storage(std::shared_ptr<storage::storage> storage) = 0;
    virtual void start(looper::loop loop) = 0;
    virtual void stop() = 0;
};

}
