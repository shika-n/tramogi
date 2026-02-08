#pragma once

#include <cstdint>
#include <glm/detail/type_quat.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>

class Camera {
public:
	Camera(uint32_t width, uint32_t height, float fov);

	const glm::vec3 &get_position() const {
		return position;
	}

	void change_perspective(uint32_t width, uint32_t height, float fov);
	void update_view();

	void rotate_to_poi(float angle_x, float angle_y);

	float get_orbit_distance() const {
		return orbit_distance;
	}
	glm::vec3 get_forward() const;
	glm::vec3 get_right() const;
	glm::vec3 get_up() const;

	glm::vec2 get_half_fov_tan() const;

	void set_position(const glm::vec3 &position);
	void set_orbit_distance(float distance);

	const glm::mat4 &get_projection() const {
		return projection;
	}
	const glm::mat4 &get_view() const {
		return view;
	}
	float get_z_near() const {
		return z_near;
	}
	float get_z_far() const {
		return z_far;
	}

private:
	float fov;
	float aspect;
	float z_near;
	float z_far;

	glm::vec3 position;
	glm::quat orientation = glm::identity<glm::quat>();
	float orbit_distance = 8.0f;

	glm::mat4 projection;
	glm::mat4 view;
};
