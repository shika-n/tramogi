#pragma once

#include "tramogi/core/types.h"
#include <cstdint>
#include <vector>

namespace tramogi::core {

Result<std::vector<uint32_t>> read_shader_file(const char *filepath);

}
