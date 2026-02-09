#pragma once

#include "../../core/heightmap.h"
#include "cube.h"
#include <cstdint>
#include <vector>

namespace tramogi::graphics::primitives {

class HeightmapTerrain {
public:
	HeightmapTerrain(
		float width,
		float depth,
		uint32_t width_resolution,
		uint32_t depth_resolution,
		float amplitude,
		const core::Heightmap &heightmap
	);

	const std::vector<engine::primitives::BasicVertex> &get_vertices() const {
		return vertices;
	}

	const std::vector<uint32_t> &get_indices() const {
		return indices;
	}

private:
	std::vector<engine::primitives::BasicVertex> vertices;
	std::vector<uint32_t> indices;
};

} // namespace tramogi::graphics::primitives
