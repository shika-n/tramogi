#pragma once

#include "tramogi/core/types.h"
#include <cstdint>
#include <memory>

namespace vk {
class Extent2D;
enum class Format;
class Image;
namespace raii {
class SwapchainKHR;
}
} // namespace vk

namespace tramogi::graphics {

class Device;
class ImageView;
class PhysicalDevice;
class SwapchainImage;

class Swapchain {
public:
	Swapchain(
		const PhysicalDevice &physical_device,
		const Device &device,
		const core::Size &window_size
	);
	~Swapchain();
	Swapchain(const Swapchain &) = delete;
	Swapchain &operator=(const Swapchain &) = delete;
	Swapchain(Swapchain &&);
	Swapchain &operator=(Swapchain &&) = delete;

	core::Result<uint32_t> get_next_image(uint32_t current_index);
	void recreate(const core::Size &window_size);

	const vk::raii::SwapchainKHR &get_swapchain() const;
	const vk::Format &get_format() const;
	const vk::Extent2D &get_extent() const;
	const SwapchainImage &get_image(uint32_t index) const;
	const ImageView &get_image_view(uint32_t index) const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;

	const PhysicalDevice &physical_device;
	const Device &device;
};

} // namespace tramogi::graphics
