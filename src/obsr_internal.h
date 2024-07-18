#pragma once

#include <pthread.h>
#include <map>
#include <string>

#include "util/handles.h"
#include "obsr.h"

namespace obsr {

constexpr size_t max_name_size = 32;
constexpr size_t max_path_size = 128;

struct object_data {
    object handle;
    std::string name;

    std::map<std::string, object , std::less<>> children;
    std::map<std::string, entry, std::less<>> entries;
};

struct entry_data {
    entry handle;
    std::string name;
    value value;
};

}
