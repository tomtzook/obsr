#pragma once

#include "storage/storage.h"
#include "events/events.h"

namespace obsr::net {

class network_interface {
public:
    virtual ~network_interface() = default;

    virtual void attach_storage(std::shared_ptr<storage::storage> storage) = 0;
    virtual void start(events::looper* looper) = 0;
    virtual void stop() = 0;
};

}
