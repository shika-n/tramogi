#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace tramogi::core {

class Heightmap {
public:
	Heightmap(uint32_t width, std::span<const float> data);

	float sample(float x, float y, float x_step, float y_step) const;

private:
	uint32_t width;
	uint32_t height;
	std::vector<float> map;
};

} // namespace tramogi::core
