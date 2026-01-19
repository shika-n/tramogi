#pragma once

#include "image_view.h"
#include <cstdint>
#include <memory>

namespace vk {
template <class T> class Flags;
enum class ImageAspectFlagBits : uint32_t;
using ImageAspectFlags = Flags<ImageAspectFlagBits>;
enum class Format;
class PhysicalDevice;
namespace raii {
class Device;
class Image;
} // namespace raii
} // namespace vk

namespace tramogi::graphics {

class CommandBuffer;
class Device;
class ImageView;
class PhysicalDevice;

class Image {
public:
	Image(
		const PhysicalDevice &physical_device,
		const Device &device,
		uint32_t width,
		uint32_t height,
		bool mipmap
	);
	virtual ~Image();

	Image(const Image &) = delete;
	Image &operator=(const Image &) = delete;
	Image(Image &&);
	Image &operator=(Image &&);

	void as_color_target(const CommandBuffer &cmd);
	void as_present_source(const CommandBuffer &cmd);

	const vk::raii::Image &get_image() const;
	vk::Format get_format() const;
	uint32_t get_mipmap_level() const {
		return mipmap_level_count;
	}
	virtual vk::ImageAspectFlags get_aspect_flags() const;

protected:
	struct Impl;
	std::unique_ptr<Impl> impl;

	uint32_t mipmap_level_count = 1;

	// Protected default ctor for different initialization of derived class
	Image();
};

class DepthImage : public Image {
public:
	DepthImage(
		const PhysicalDevice &physical_device,
		const Device &device,
		uint32_t width,
		uint32_t height,
		bool mipmap
	);
	~DepthImage() = default;

	DepthImage(const DepthImage &) = delete;
	DepthImage &operator=(const DepthImage &) = delete;
	DepthImage(DepthImage &&) = default;
	DepthImage &operator=(DepthImage &&) = default;

	void as_depth_target(const CommandBuffer &cmd);
	vk::ImageAspectFlags get_aspect_flags() const;
};

} // namespace tramogi::graphics
