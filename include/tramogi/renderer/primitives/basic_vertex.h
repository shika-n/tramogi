#pragma once

#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>

namespace tramogi::renderer::primitives {

struct BasicVertex {
	glm::vec3 position;
	glm::vec3 color;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec2 uv;
};

} // namespace tramogi::renderer::primitives
