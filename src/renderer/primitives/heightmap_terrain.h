#pragma once

#include "../../core/heightmap.h"
#include "tramogi/renderer/mesh.h"
#include <cstdint>

namespace tramogi::renderer::primitives {

class HeightmapTerrainMesh : public Mesh {
public:
	HeightmapTerrainMesh(
		float width,
		float depth,
		uint32_t width_resolution,
		uint32_t depth_resolution,
		float amplitude,
		const core::Heightmap &heightmap
	);
};

} // namespace tramogi::renderer::primitives
