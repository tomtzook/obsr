#include <sys/eventfd.h>

#include "internal_except.h"
#include "signal.h" // NOLINT(*-deprecated-headers)

namespace obsr::os {

static os::descriptor create() {
    auto fd = eventfd(0, EFD_NONBLOCK);
    if (fd < 0) {
        throw io_exception(errno);
    }

    return fd;
}

signal::signal()
    : resource(create())
{}

void signal::set() {
    ::eventfd_write(get_descriptor(), 1);
}

void signal::clear() {
    eventfd_t value;
    ::eventfd_read(get_descriptor(), &value);
}

}
