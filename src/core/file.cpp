#include "tramogi/core/errors.h"
#include <fstream>
#include <vector>

namespace tramogi::core {

Result<std::vector<char>> read_shader_file(const char *filepath) {
	std::ifstream file(filepath, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		return Error("Failed to open shader file");
	}

	std::vector<char> buffer(file.tellg());
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

	file.close();

	return buffer;
}

} // namespace tramogi::core
