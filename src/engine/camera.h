#pragma once

#include <cstdint>
#include <glm/detail/type_quat.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>

namespace tramogi::engine {

class Camera {
public:
	Camera(uint32_t width, uint32_t height, float fov);

	const glm::vec3 &get_position() const {
		return position;
	}

	void change_perspective(uint32_t width, uint32_t height, float fov);
	void update_view();

	void recalculate_position();
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
	void set_point_of_interest(const glm::vec3 &point_of_interest) {
		this->point_of_interest = point_of_interest;
		recalculate_position();
	}

	const glm::vec3 &get_point_of_interest() const {
		return point_of_interest;
	}
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
	glm::vec3 point_of_interest = glm::vec3(0.0f);
	glm::quat orientation = glm::identity<glm::quat>();
	float orbit_distance = 5.0f;

	glm::mat4 projection;
	glm::mat4 view;
};

class OrthoCamera {
public:
	OrthoCamera(float width, float height, float z_near, float z_far);

	const glm::vec3 &get_position() const {
		return position;
	}

	void change_perspective(uint32_t width, uint32_t height, float fov);
	void update_view();

	glm::vec3 get_forward() const;
	glm::vec3 get_right() const;
	glm::vec3 get_up() const;

	void set_position(const glm::vec3 &position);
	void set_orientation(const glm::quat &orientation) {
		this->orientation = orientation;
	}

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
	float aspect;
	float z_near;
	float z_far;

	glm::vec3 position;
	glm::quat orientation = glm::identity<glm::quat>();

	glm::mat4 projection;
	glm::mat4 view;
};

} // namespace tramogi::engine
