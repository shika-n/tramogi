#pragma once

#include "tramogi/core/types.h"
#include <vector>

namespace tramogi::core {

Result<std::vector<char>> read_shader_file(const char *filepath);

}
