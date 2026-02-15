#pragma once

#include "tramogi/graphics/primitives/basic_vertex.h"
#include <cstdint>
#include <vector>

namespace tramogi::core {

using engine::primitives::BasicVertex;

class Model {
public:
	bool load_from_obj_file(const char *filepath);

	const std::vector<BasicVertex> get_vertices() const {
		return vertices;
	}
	const std::vector<uint32_t> get_indices() const {
		return indices;
	}

private:
	std::vector<BasicVertex> vertices;
	std::vector<uint32_t> indices;
};

} // namespace tramogi::core

