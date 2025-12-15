#pragma once

#include <expected>
#include <vector>

namespace tramogi::core {

std::expected<std::vector<char>, const char *> read_shader_file(const char *filepath);

}
