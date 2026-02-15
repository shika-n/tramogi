#include "tramogi/core/io/model.h"

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/ext/vector_float3.hpp>
#include <stdint.h>
#include <tiny_obj_loader.h>

#include <glm/gtx/hash.hpp>

#include "tramogi/core/logging/logging.h"
#include "tramogi/graphics/primitives/basic_vertex.h"

namespace tramogi::core {

bool Model::load_from_obj_file(const char *filepath) {
	logging::debug_log("Loading model {}", filepath);
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn;
	std::string err;
	bool res = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath);
	logging::debug_log("{}", warn + err);

	// std::unordered_map<BasicVertex, uint32_t> unique_vertices;
	for (const auto &shape : shapes) {
		for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
			for (size_t j = 0; j < 3; ++j) {
				const auto &index = shape.mesh.indices[i + 2 - j];
				glm::vec2 uv = {0, 0};
				if (attrib.texcoords.size() > 0) {
					uv = {
						attrib.texcoords[2 * index.texcoord_index + 0],
						1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
					};
				}
				BasicVertex vertex {
					.position =
						{
							attrib.vertices[3 * index.vertex_index + 0],
							attrib.vertices[3 * index.vertex_index + 1],
							attrib.vertices[3 * index.vertex_index + 2],
						},
					.color = {1, 1, 1},
					.normal = {0, 1, 0},
					.uv = uv,
				};

				indices.push_back(vertices.size());
				vertices.push_back(vertex);
			}
		}
	}

	for (size_t i = 0; i < vertices.size(); i += 3) {
		glm::vec3 edge1 = vertices[i].position - vertices[i + 1].position;
		glm::vec3 edge2 = vertices[i].position - vertices[i + 2].position;
		glm::vec3 normal = glm::cross(glm::normalize(edge2), glm::normalize(edge1));

		vertices[i].normal = normal;
		vertices[i + 1].normal = normal;
		vertices[i + 2].normal = normal;
	}

	logging::debug_log("{} {}", vertices.size(), indices.size());

	return res;
}

} // namespace tramogi::core
