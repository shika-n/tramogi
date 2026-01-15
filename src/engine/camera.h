#pragma once

#include <cstdint>
#include <glm/detail/type_quat.hpp>
#include <glm/ext/matrix_float4x4.hpp>
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

	void set_position(const glm::vec3 &position);
	void set_point_of_interest(const glm::vec3 &position);

	const glm::mat4 &get_projection() const {
		return projection;
	}
	const glm::mat4 &get_view() const {
		return view;
	}

private:
	glm::vec3 position;
	glm::quat orientation;

	glm::mat4 projection;
	glm::mat4 view;
};
