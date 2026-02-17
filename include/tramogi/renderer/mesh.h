#pragma once

#include "tramogi/graphics/buffer.h"
#include "tramogi/renderer/primitives/basic_vertex.h"
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace tramogi::renderer {

class Mesh {
public:
	bool load_from_obj_file(const char *filepath);

	void set_vertices(std::initializer_list<primitives::BasicVertex> vertices);
	void set_vertices(std::vector<primitives::BasicVertex> &&vertices);
	void set_indices(std::initializer_list<uint32_t> indices);
	void set_indices(std::vector<uint32_t> &&indices);

	void set_vertex_buffer(graphics::VertexBuffer *vertex_buffer) {
		this->vertex_buffer = vertex_buffer;
	}

	void set_index_buffer(graphics::IndexBuffer *index_buffer) {
		this->index_buffer = index_buffer;
	}

	const std::vector<primitives::BasicVertex> &get_vertices() const {
		return vertices;
	}
	const std::vector<uint32_t> &get_indices() const {
		return indices;
	}

	const graphics::VertexBuffer *get_vertex_buffer() const {
		return vertex_buffer;
	}

	const graphics::IndexBuffer *get_index_buffer() const {
		return index_buffer;
	}

private:
	std::vector<primitives::BasicVertex> vertices;
	std::vector<uint32_t> indices;

	graphics::VertexBuffer *vertex_buffer = nullptr;
	graphics::IndexBuffer *index_buffer = nullptr;
};

} // namespace tramogi::renderer
