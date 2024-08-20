#pragma once

#include "updater.h"
#include "storage/storage.h"

namespace obsr::net {

class network_interface : public updatable {
public:
    virtual void attach_storage(std::shared_ptr<storage::storage> storage) = 0;
    virtual void stop() = 0;
};

}
