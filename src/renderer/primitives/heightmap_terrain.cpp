#include "heightmap_terrain.h"
#include "tramogi/renderer/primitives/basic_vertex.h"
#include <cstdint>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <vector>

namespace tramogi::renderer::primitives {

HeightmapTerrainMesh::HeightmapTerrainMesh(
	float width,
	float depth,
	uint32_t width_resolution,
	uint32_t depth_resolution,
	float amplitude,
	const core::Heightmap &heightmap
) {
	uint32_t tile_count = (width_resolution - 1) * (depth_resolution - 1);
	std::vector<BasicVertex> vertices;
	std::vector<uint32_t> indices;
	vertices.resize(tile_count * 6);

	float tile_w = static_cast<float>(width) / width_resolution;
	float tile_d = static_cast<float>(depth) / depth_resolution;
	float sample_step_x = 1.0f / width_resolution;
	float sample_step_y = 1.0f / depth_resolution;

	glm::vec3 color {1.0, 1.0, 1.0};

	for (size_t z = 0; z < depth_resolution - 1; ++z) {
		for (size_t x = 0; x < width_resolution - 1; ++x) {
			uint32_t tile_i = (x + z * (width_resolution - 1)) * 6;
			float tile_x = x;
			float tile_z = z;

			float height_ul = amplitude * heightmap.sample(
											  (tile_x) / width_resolution,
											  (tile_z) / depth_resolution,
											  sample_step_x,
											  sample_step_y
										  );
			float height_ur = amplitude * heightmap.sample(
											  (tile_x + 1) / width_resolution,
											  (tile_z) / depth_resolution,
											  sample_step_x,
											  sample_step_y
										  );
			float height_br = amplitude * heightmap.sample(
											  (tile_x + 1) / width_resolution,
											  (tile_z + 1) / depth_resolution,
											  sample_step_x,
											  sample_step_y
										  );
			float height_bl = amplitude * heightmap.sample(
											  (tile_x) / width_resolution,
											  (tile_z + 1) / depth_resolution,
											  sample_step_x,
											  sample_step_y
										  );

			float uv_multiplier = 20.0f;
			glm::vec2 uv_ul = glm::vec2((tile_x) / width_resolution, (tile_z) / depth_resolution) *
							  uv_multiplier;
			glm::vec2 uv_ur =
				glm::vec2((tile_x + 1) / width_resolution, (tile_z) / depth_resolution) *
				uv_multiplier;
			glm::vec2 uv_bl =
				glm::vec2((tile_x) / width_resolution, (tile_z + 1) / depth_resolution) *
				uv_multiplier;
			glm::vec2 uv_br =
				glm::vec2((tile_x + 1) / width_resolution, (tile_z + 1) / depth_resolution) *
				uv_multiplier;

			glm::vec3 normal0 = glm::cross(
				glm::normalize(glm::vec3(0, height_br - height_ur, -tile_d)),
				glm::normalize(glm::vec3(-tile_w, height_ul - height_ur, 0))
			);
			glm::vec3 normal1 = glm::cross(
				glm::normalize(glm::vec3(0, height_ul - height_bl, tile_d)),
				glm::normalize(glm::vec3(tile_w, height_br - height_bl, 0))
			);

			glm::vec3 pos_ul = glm::vec3(
				tile_x * tile_w - (width / 2),
				height_ul,
				-tile_z * tile_d - (-depth / 2)
			);
			glm::vec3 pos_ur = glm::vec3(
				(tile_x + 1) * tile_w - (width / 2),
				height_ur,
				-tile_z * tile_d - (-depth / 2)
			);
			glm::vec3 pos_bl = glm::vec3(
				tile_x * tile_w - (width / 2),
				height_bl,
				-(tile_z + 1) * tile_d - (-depth / 2)
			);
			glm::vec3 pos_br = glm::vec3(
				(tile_x + 1) * tile_w - (width / 2),
				height_br,
				-(tile_z + 1) * tile_d - (-depth / 2)
			);

			vertices[tile_i + 0] = {
				.position = pos_ul,
				.color = color,
				.normal = normal0,
				.uv = uv_ul,
			};
			vertices[tile_i + 1] = {
				.position = pos_br,
				.color = color,
				.normal = normal0,
				.uv = uv_br,
			};
			vertices[tile_i + 2] = {
				.position = pos_ur,
				.color = color,
				.normal = normal0,
				.uv = uv_ur,
			};
			vertices[tile_i + 3] = {
				.position = pos_ul,
				.color = color,
				.normal = normal1,
				.uv = uv_ul,
			};
			vertices[tile_i + 4] = {
				.position = pos_bl,
				.color = color,
				.normal = normal1,
				.uv = uv_bl,
			};
			vertices[tile_i + 5] = {
				.position = pos_br,
				.color = color,
				.normal = normal1,
				.uv = uv_br,
			};
			indices.push_back(tile_i + 0);
			indices.push_back(tile_i + 1);
			indices.push_back(tile_i + 2);
			indices.push_back(tile_i + 3);
			indices.push_back(tile_i + 4);
			indices.push_back(tile_i + 5);
		}
	}

	set_vertices(std::move(vertices));
	set_indices(std::move(indices));
} // namespace tramogi::graphics::primitives

} // namespace tramogi::renderer::primitives
