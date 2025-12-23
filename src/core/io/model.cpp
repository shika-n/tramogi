#include "tramogi/core/io/model.h"

#include <stdint.h>
#include <tiny_obj_loader.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "../../logging.h"

namespace std {

template <> struct hash<tramogi::core::Vertex> {
	size_t operator()(const tramogi::core::Vertex &vertex) const {
		return hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec2>()(vertex.tex_coord));
	}
};

} // namespace std

namespace tramogi::core {

bool Vertex::operator==(const Vertex &other) const {
	return position == other.position && tex_coord == other.tex_coord;
}

bool Model::load_from_obj_file(const char *filepath) {
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn;
	std::string err;
	bool res = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath);
	DLOG("{}", warn + err);

	std::unordered_map<Vertex, uint32_t> unique_vertices;
	for (const auto &shape : shapes) {
		for (const auto &index : shape.mesh.indices) {
			Vertex vertex {
				.position =
					{
						attrib.vertices[3 * index.vertex_index + 0],
						attrib.vertices[3 * index.vertex_index + 1],
						attrib.vertices[3 * index.vertex_index + 2],
					},
				.tex_coord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
				},
			};

			if (unique_vertices.count(vertex) == 0) {
				unique_vertices[vertex] = static_cast<glm::uint>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(unique_vertices[vertex]);
		}
	}

	return res;
}

} // namespace tramogi::core
