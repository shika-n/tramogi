#pragma once

#include "image_view.h"
#include "tramogi/core/pointers.h"
#include <cstdint>

namespace vk {
template <class T> class Flags;
enum class ImageAspectFlagBits : uint32_t;
using ImageAspectFlags = Flags<ImageAspectFlagBits>;
class PhysicalDevice;
namespace raii {
class Device;
class Image;
} // namespace raii
} // namespace vk

namespace tramogi::graphics {

class CommandBuffer;
class Device;
enum class Format;
class ImageView;
class PhysicalDevice;

class Image {
public:
	enum class Usage {
		SampledColorTarget,
		SampledDepth,
		Texture,
		CubeMap,
	};
	Image(
		const PhysicalDevice &physical_device,
		const Device &device,
		uint32_t width,
		uint32_t height,
		Format format,
		Usage usage,
		bool mipmap
	);
	virtual ~Image();

	Image(const Image &) = delete;
	Image &operator=(const Image &) = delete;
	Image(Image &&);
	Image &operator=(Image &&);

	void generate_mipmap(const CommandBuffer &cmd);

	void as_color_target(const CommandBuffer &cmd) const;
	void as_transfer_src(const CommandBuffer &cmd) const;
	void as_transfer_dst(const CommandBuffer &cmd) const;
	void as_sampled(const CommandBuffer &cmd) const;

	const vk::raii::Image &get_image() const;
	Format get_format() const;
	uint32_t get_mipmap_level() const {
		return mipmap_level_count;
	}
	Usage get_usage() const {
		return usage;
	}
	virtual vk::ImageAspectFlags get_aspect_flags() const;

protected:
	struct Impl;
	core::UniquePtr<Impl> impl;

	uint32_t width;
	uint32_t height;

	Usage usage;
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

	void as_depth_target(const CommandBuffer &cmd) const;
	vk::ImageAspectFlags get_aspect_flags() const;
};

class SwapchainImage {
public:
	SwapchainImage(vk::Image image);
	~SwapchainImage();
	SwapchainImage(const SwapchainImage &) = delete;
	SwapchainImage &operator=(const SwapchainImage &) = delete;
	SwapchainImage(SwapchainImage &&);
	SwapchainImage &operator=(SwapchainImage &&);

	void as_attachment(const CommandBuffer &cmd) const;
	void as_present_source(const CommandBuffer &cmd) const;

	vk::Image get_image() const;

private:
	struct Impl;
	core::UniquePtr<Impl> impl;
};

class CubeMapImage : public Image {
public:
	CubeMapImage(
		const PhysicalDevice &physical_device,
		const Device &device,
		uint32_t width,
		uint32_t height,
		Format format,
		bool mipmap
	);
	~CubeMapImage();
	CubeMapImage(const CubeMapImage &) = delete;
	CubeMapImage &operator=(const CubeMapImage &) = delete;
	CubeMapImage(CubeMapImage &&);
	CubeMapImage &operator=(CubeMapImage &&);
};

} // namespace tramogi::graphics
