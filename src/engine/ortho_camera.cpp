#include "camera.h"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace tramogi::engine {

OrthoCamera::OrthoCamera(
	[[maybe_unused]] float width,
	[[maybe_unused]] float height,
	[[maybe_unused]] float z_near,
	[[maybe_unused]] float z_far
)
	: z_near(z_near), z_far(z_far) {
	projection = glm::ortho(-120.0f, 120.0f, 120.0f, -120.0f, -150.0f, 150.0f);
}

// --- Below has the same code as Camera, might need to refactor ---
void OrthoCamera::update_view() {
	view = glm::translate(glm::mat4_cast(glm::conjugate(orientation)), -position);
}

glm::vec3 OrthoCamera::get_forward() const {
	return orientation * glm::vec3(0, 0, 1);
}

glm::vec3 OrthoCamera::get_right() const {
	return orientation * glm::vec3(1, 0, 0);
}

glm::vec3 OrthoCamera::get_up() const {
	return orientation * glm::vec3(0, 1, 0);
}

void OrthoCamera::set_position(const glm::vec3 &position) {
	this->position = position;
}

} // namespace tramogi::engine
