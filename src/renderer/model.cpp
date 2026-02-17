#include "tramogi/renderer/model.h"

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <tiny_obj_loader.h>

namespace tramogi::renderer {

glm::mat4 Model::get_transform_matrix() const {
	return glm::scale(
		glm::translate(glm::identity<glm::mat4>(), position) * glm::mat4_cast(orientation),
		scale
	);
}

} // namespace tramogi::renderer
