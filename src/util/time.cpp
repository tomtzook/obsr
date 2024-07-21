
#include "time.h" // NOLINT(*-deprecated-headers)

namespace obsr {

std::chrono::milliseconds time_now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());
}

}
