#pragma once

#include "tramogi/renderer/mesh.h"
#include <cstdint>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <string_view>

namespace tramogi::renderer {

class Model {
public:
	Model(uint32_t id, Mesh *mesh) : id(id), mesh(mesh) {}

	void set_position(const glm::vec3 &position) {
		this->position = position;
	}

	void set_orientation(const glm::quat &orientation) {
		this->orientation = orientation;
	}

	void set_scale(const glm::vec3 &scale) {
		this->scale = scale;
	}

	void set_name(std::string_view name) {
		this->name = name;
	}

	void set_texture_id(uint32_t texture_id) {
		this->texture_id = texture_id;
	}

	void set_metalness(float metalness) {
		this->metalness = metalness;
	}

	Mesh *get_mesh() const {
		return mesh;
	}

	uint32_t get_id() const {
		return id;
	}

	std::string_view get_name() const {
		return name;
	}

	uint32_t get_texture_id() const {
		return texture_id;
	}

	glm::mat4 get_transform_matrix() const;

	const glm::vec3 &get_position() const {
		return position;
	}

	const glm::quat &get_orientation() const {
		return orientation;
	}

	const glm::vec3 &get_scale() const {
		return scale;
	}

	float get_metalness() const {
		return metalness;
	}

private:
	uint32_t id = 0;
	std::string name = "Object";
	Mesh *mesh = nullptr;

	uint32_t texture_id = 0; // Should move this somewhere else

	glm::vec3 position = glm::vec3(0.0f);
	glm::quat orientation = glm::identity<glm::quat>();
	glm::vec3 scale = glm::vec3(1.0f);

	float metalness = 0.0f;
};

} // namespace tramogi::renderer
