#pragma once

#include "events/signal.h"
#include "os/io.h"

namespace obsr::os {

class signal : public events::signal, public os::resource {
public:
    signal();

    void set() override;
    void clear() override;
};

}
