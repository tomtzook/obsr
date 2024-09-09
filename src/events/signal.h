#pragma once

#include "obsr_types.h"

namespace obsr::events {

class signal {
public:
    virtual ~signal() = default;
    virtual void set() = 0;
    virtual void clear() = 0;
};

}
