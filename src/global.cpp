
#include "global.h"

namespace obsr {

instance& global_instance() {
    static instance s_instance{};
    return s_instance;
}

}
