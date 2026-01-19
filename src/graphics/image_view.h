#pragma once

#include <concepts>
#include <memory>

namespace vk::raii {
class ImageView;
}

namespace tramogi::graphics {

class Device;
class Image;
class PhysicalDevice;

class ImageView {
public:
	ImageView(const Device &device, const Image &image);
	~ImageView();
	ImageView(const ImageView &) = delete;
	ImageView &operator=(const ImageView &) = delete;
	ImageView(ImageView &&);
	ImageView &operator=(ImageView &&);

	const vk::raii::ImageView &get_image_view() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

template <class T, class U>
concept Derived = std::derived_from<T, U>;
template <Derived<Image> T> class ImageViewPair {
public:
	ImageViewPair(
		const PhysicalDevice &physical_device,
		const Device &device,
		uint32_t width,
		uint32_t height,
		bool mipmap
	)
		: image(physical_device, device, width, height, mipmap), image_view(device, image) {}
	~ImageViewPair() = default;

	ImageViewPair(const ImageViewPair &) = delete;
	ImageViewPair &operator=(const ImageViewPair &) = delete;
	ImageViewPair(ImageViewPair &&) = default;
	ImageViewPair &operator=(ImageViewPair &&) = default;

	T &get_image() {
		return image;
	}

	ImageView &get_image_view() {
		return image_view;
	}

private:
	T image;
	ImageView image_view;
};

} // namespace tramogi::graphics
