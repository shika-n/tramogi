#include "tramogi/renderer/mesh.h"
#include "tramogi/core/logging/logging.h"

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/geometric.hpp>
#include <tiny_obj_loader.h>

namespace tramogi::renderer {

bool Mesh::load_from_obj_file(const char *filepath) {
	core::logging::debug_log("Loading model: {}", filepath);

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn;
	std::string err;
	bool res = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath);

	bool has_uv = attrib.texcoords.size() > 0;
	bool has_normals = attrib.normals.size() > 0;

	for (const auto &shape : shapes) {
		for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
			for (size_t j = 0; j < 3; ++j) {
				const auto &index = shape.mesh.indices[i + 2 - j];
				glm::vec2 uv = {0, 0};
				glm::vec3 normal = {0, 0, 0};
				if (has_normals) {
					normal = glm::normalize(
						glm::vec3(
							attrib.normals[3 * index.normal_index + 0],
							attrib.normals[3 * index.normal_index + 1],
							attrib.normals[3 * index.normal_index + 2]
						)
					);
				}
				if (has_uv) {
					uv = {
						attrib.texcoords[2 * index.texcoord_index + 0],
						1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
					};
				}
				primitives::BasicVertex vertex {
					.position =
						{
							attrib.vertices[3 * index.vertex_index + 0],
							attrib.vertices[3 * index.vertex_index + 1],
							attrib.vertices[3 * index.vertex_index + 2],
						},
					.color = {1, 1, 1},
					.normal = normal,
					.uv = uv,
				};

				indices.push_back(vertices.size());
				vertices.push_back(vertex);
			}
		}
	}

	if (!has_normals) {
		for (size_t i = 0; i < vertices.size(); i += 3) {
			glm::vec3 edge1 = vertices[i].position - vertices[i + 1].position;
			glm::vec3 edge2 = vertices[i].position - vertices[i + 2].position;
			glm::vec3 normal = glm::cross(glm::normalize(edge2), glm::normalize(edge1));

			vertices[i].normal = normal;
			vertices[i + 1].normal = normal;
			vertices[i + 2].normal = normal;
		}
	}

	core::logging::debug_log("Model loaded");
	return res;
}

void Mesh::set_vertices(std::initializer_list<primitives::BasicVertex> vertices) {
	this->vertices = vertices;
}
void Mesh::set_vertices(std::vector<primitives::BasicVertex> &&vertices) {
	this->vertices = std::move(vertices);
}

void Mesh::set_indices(std::initializer_list<uint32_t> indices) {
	this->indices = indices;
}
void Mesh::set_indices(std::vector<uint32_t> &&indices) {
	this->indices = std::move(indices);
}

} // namespace tramogi::renderer
