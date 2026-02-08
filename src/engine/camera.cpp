#include "camera.h"
#include <cmath>
#include <cstdint>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

Camera::Camera(uint32_t width, uint32_t height, float fov)
	: fov(fov), aspect(static_cast<float>(width) / height) {
	change_perspective(width, height, fov);
	update_view();
}

void Camera::change_perspective(uint32_t width, uint32_t height, float fov) {
	this->fov = fov;
	this->aspect = static_cast<float>(width) / height;
	z_near = 0.1f;
	z_far = 100.0f;
	projection = glm::perspective(
		fov,
		static_cast<float>(width) / static_cast<float>(height),
		z_near,
		z_far
	);
	projection[1][1] *= -1;
}

void Camera::update_view() {
	view = glm::translate(glm::mat4_cast(glm::conjugate(orientation)), -position);
}

void Camera::rotate_to_poi(float angle_x, float angle_y) {
	bool is_flipped = std::signbit((orientation * glm::vec3(0, 1, 0)).y);
	if (is_flipped) {
		angle_y *= -1;
	}

	glm::qua x_rot = glm::angleAxis(angle_x, glm::normalize(orientation * glm::vec3(1, 0, 0)));
	glm::qua y_rot = glm::angleAxis(angle_y, glm::vec3(0, 1, 0));
	orientation = y_rot * x_rot * orientation;

	glm::vec3 forward = glm::normalize(orientation * glm::vec3(0, 0, 1));
	position = forward * -orbit_distance;
}

glm::vec3 Camera::get_forward() const {
	return orientation * glm::vec3(0, 0, 1);
}

glm::vec3 Camera::get_right() const {
	return orientation * glm::vec3(1, 0, 0);
}

glm::vec3 Camera::get_up() const {
	return orientation * glm::vec3(0, 1, 0);
}

glm::vec2 Camera::get_half_fov_tan() const {
	float half_yfov_tan = glm::tan(fov / 2.0);
	return glm::vec2(half_yfov_tan * aspect, half_yfov_tan);
}

void Camera::set_position(const glm::vec3 &position) {
	this->position = position;
}

void Camera::set_orbit_distance(float distance) {
	orbit_distance = std::max(distance, 0.0f);
	rotate_to_poi(0, 0);
}
