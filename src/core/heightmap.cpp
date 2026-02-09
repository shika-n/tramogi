#include "heightmap.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <span>

namespace tramogi::core {

Heightmap::Heightmap(uint32_t width, std::span<const float> data)
	: width(width), height(data.size() / width) {

	map.resize(width * height);

	for (size_t i = 0; i < map.size(); ++i) {
		map[i] = data[i];
	}
}

float Heightmap::sample(float x, float y, float x_step, float y_step) const {
	assert(map.size() > 0 && "The heightmap has no data");

	uint32_t x0 = std::floor(x * (width - 1));
	uint32_t x1 =
		std::min(static_cast<uint32_t>(std::floor((x + x_step) * (width - 1))), width - 1);
	uint32_t y0 = std::floor(y * (height - 1));
	uint32_t y1 =
		std::min(static_cast<uint32_t>(std::floor((y + y_step) * (height - 1))), height - 1);

	float h0 = std::lerp(map[x0 + y0 * width], map[x1 + y0 * width], x - std::floor(x));
	float h1 = std::lerp(map[x0 + y1 * width], map[x1 + y1 * width], x - std::floor(x));
	float sampled = std::lerp(h0, h1, y - std::floor(y));

	return sampled;
}

} // namespace tramogi::core
