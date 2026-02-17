#pragma once

#include "image_view.h"
#include <array>
#include <cstdint>
#include <utility>

namespace tramogi::graphics {

class Device;
class PhysicalDevice;

template <class T> class InFlightSet {
public:
	template <class... Args>
		requires std::constructible_from<T, Args...>
	InFlightSet(Args &&...args)
		: images({
			  ImageViewPair<T>(std::forward<Args>(args)...),
			  ImageViewPair<T>(std::forward<Args>(args)...),
		  }) {}

	T &get_image(uint32_t frame_index) {
		return images[frame_index].get_image();
	}
	ImageView &get_image_view(uint32_t frame_index) {
		return images[frame_index].get_image_view();
	}

private:
	std::array<ImageViewPair<T>, 2> images;
};

} // namespace tramogi::graphics
