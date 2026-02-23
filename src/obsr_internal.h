#pragma once

#include <pthread.h>
#include <map>
#include <string>

#include "util/handles.h"
#include "obsr.h"

namespace obsr {

using client_id = uint16_t;
static constexpr client_id invalid_client_id = static_cast<client_id>(-1);

namespace storage {
using entry_id = uint16_t;
constexpr entry_id id_not_assigned = static_cast<entry_id>(-1);
}
}
