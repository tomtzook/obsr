#pragma once

#include "updater.h"
#include "storage/storage.h"

namespace obsr::net {

class network_interface {
public:
    virtual void attach_storage(std::shared_ptr<storage::storage> storage) = 0;
    virtual void start(events::looper* looper) = 0;
    virtual void stop() = 0;
};

}
