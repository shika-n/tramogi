#pragma once

#include <cstdint>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <vector>

namespace tramogi::core {

struct Vertex {
	glm::vec3 position;
	glm::vec2 tex_coords;
};

class Model {
public:
	void load_from();

private:
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

class ObjLoader {
public:
	bool load_from_file(const char *filepath);

private:
};

} // namespace tramogi::core
