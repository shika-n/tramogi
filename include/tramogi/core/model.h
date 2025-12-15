#pragma once

#include <cstdint>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <vector>

namespace tramogi::core {

struct Vertex {
	glm::vec3 position;
	glm::vec2 tex_coord;

	bool operator==(const Vertex &other) const;
};

class Model {
public:
	bool load_from_obj_file(const char *filepath);

	const std::vector<Vertex> get_vertices() const {
		return vertices;
	}
	const std::vector<uint32_t> get_indices() const {
		return indices;
	}

private:
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

} // namespace tramogi::core

