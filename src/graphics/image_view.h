#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <utility>

namespace vk {
class Image;
enum class Format;
template <class T> class Flags;
enum class ImageAspectFlagBits : uint32_t;
using ImageAspectFlags = Flags<ImageAspectFlagBits>;
namespace raii {
class ImageView;
}
} // namespace vk

namespace tramogi::graphics {

class Device;
enum class Format;
class Image;
class PhysicalDevice;
class SwapchainImage;

class ImageView {
public:
	ImageView(const Device &device, const Image &image);
	ImageView(
		const Device &device,
		const SwapchainImage &image,
		Format format,
		vk::ImageAspectFlags aspect_flags,
		uint32_t mipmap_level_count
	);
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
	template <class... Args>
		requires std::constructible_from<T, const PhysicalDevice &, const Device &, Args...>
	ImageViewPair(const PhysicalDevice &physical_device, const Device &device, Args &&...args)
		: image(physical_device, device, std::forward<Args>(args)...), image_view(device, image) {}
	~ImageViewPair() = default;

	ImageViewPair(const ImageViewPair &) = delete;
	ImageViewPair &operator=(const ImageViewPair &) = delete;
	ImageViewPair(ImageViewPair &&) = default;
	ImageViewPair &operator=(ImageViewPair &&) = default;

	const T &get_image() const {
		return image;
	}

	const ImageView &get_image_view() const {
		return image_view;
	}

private:
	T image;
	ImageView image_view;
};

} // namespace tramogi::graphics
