#include "tramogi/core/logging/logging.h"
#include "tramogi/core/types.h"
#include <cstdint>
#include <fstream>
#include <vector>

namespace tramogi::core {

Result<std::vector<uint32_t>> read_shader_file(const char *filepath) {
	std::ifstream file(filepath, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		return Error("Failed to open shader file");
	}

	std::vector<uint32_t> buffer(file.tellg() / sizeof(uint32_t));
	file.seekg(0, std::ios::beg);
	file.read(
		reinterpret_cast<char *>(buffer.data()),
		static_cast<std::streamsize>(buffer.size() * sizeof(uint32_t))
	);

	file.close();

	return buffer;
}

} // namespace tramogi::core
