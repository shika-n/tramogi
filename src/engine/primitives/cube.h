#pragma once

#include <cstdint>
#include <glm/ext/vector_float3.hpp>
#include <vector>

namespace tramogi::engine::primitives {

struct BasicVertex {
	glm::vec3 position;
	glm::vec3 color;
};

class Cube {
public:
	Cube(float size);

	const std::vector<BasicVertex> &get_vertices() const {
		return vertices;
	}

	const std::vector<uint32_t> &get_indices() const {
		return indices;
	}

private:
	std::vector<BasicVertex> vertices {
		// Front
		{{-0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
		{{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},

		// Back
		{{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
		{{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},

		// Top
		{{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
		{{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},

		// Bottom
		{{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},

		// Left
		{{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
		{{-0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
		{{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},

		// Right
		{{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
	};
	// clang-format off
	std::vector<uint32_t> indices {
		0, 1, 2, 3, 2, 1, // Front
		4, 5, 6, 7, 6, 5, // Back
		8, 9, 10, 11, 10, 9, // Top
		12, 13, 14, 15, 14, 13, // Bottom
		16, 17, 18, 19, 18, 17, // Left
		20, 21, 22, 23, 22, 21, // Right
	};
	// clang-format on
};

} // namespace tramogi::engine::primitives
