#include "tramogi/core/io/image_data.h"
#include "tramogi/core/types.h"
#include <cstdint>
#include <format>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace tramogi::core {

void ImageData::cleanup() {
	if (data) {
		stbi_image_free(data);
		data = nullptr;
		width = 0;
		height = 0;
		channels = 0;
	}
}

ImageData::~ImageData() {
	cleanup();
}

uint32_t ImageData::get_mip_levels() const {
	return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

uint32_t ImageData::get_size() const {
	return width * height * channels;
}

Result<> ImageData::load_from_file(const char *filepath) {
	cleanup();

	int width = 0;
	int height = 0;
	stbi_uc *pixels = stbi_load(filepath, &width, &height, nullptr, STBI_rgb_alpha);
	if (!pixels) {
		return Error(std::format("File doesn't exists or is an unsupported format ({})", filepath));
	}

	data = pixels;
	this->width = width;
	this->height = height;
	this->channels = 4;

	return {};
}

Result<> ImageData::load_heightmap_from_file(const char *filepath) {
	cleanup();

	int width = 0;
	int height = 0;
	float *pixels = stbi_loadf(filepath, &width, &height, nullptr, STBI_grey);
	if (!pixels) {
		return Error(std::format("File doesn't exists or is an unsupported format ({})", filepath));
	}

	data = pixels;
	this->width = width;
	this->height = height;
	this->channels = 1;

	return {};
}

} // namespace tramogi::core
