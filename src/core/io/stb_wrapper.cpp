#include "tramogi/core/io/image_data.h"
#include <cstdint>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace tramogi::core {

ImageData::~ImageData() {
	if (data) {
		stbi_image_free(data);
	}
}

uint32_t ImageData::get_mip_levels() const {
	return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

uint32_t ImageData::get_size() const {
	return width * height * 4;
}

bool ImageData::load_from_file(const char *filepath) {
	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_uc *pixels = stbi_load(filepath, &width, &height, &channels, STBI_rgb_alpha);
	if (!pixels) {
		return false;
	}

	data = pixels;
	this->width = width;
	this->height = height;
	this->channels = channels;

	return true;
}

} // namespace tramogi::core
