
#include <unistd.h>

#include "internal_except.h"
#include "io.h"

namespace obsr::os {

resource::resource(descriptor resource_descriptor)
    : m_descriptor(resource_descriptor)
{}

resource::~resource() {
    close();
}

void resource::close() {
    if (m_descriptor >= 0) {
        ::close(m_descriptor);
        m_descriptor = -1;
    }
}

void resource::throw_if_closed() const {
    if (m_descriptor < 0) {
        throw closed_fd_exception();
    }
}

}
