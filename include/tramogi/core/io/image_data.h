#pragma once

#include <cstdint>

namespace tramogi::core {

class ImageData {
public:
	ImageData() = default;
	ImageData(const ImageData &) = delete;
	void operator=(const ImageData &) = delete;
	~ImageData();

	bool load_from_file(const char *filepath);

	uint32_t get_mip_levels() const;
	uint32_t get_size() const;

	const void *get_data() const {
		return data;
	}
	int get_width() const {
		return width;
	}
	int get_height() const {
		return height;
	}
	int get_channels() const {
		return channels;
	}

private:
	void *data = nullptr;
	int width = 0;
	int height = 0;
	int channels = 0;
};

} // namespace tramogi::core
