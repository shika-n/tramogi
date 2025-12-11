#pragma once

#include "tramogi/core/errors.h"
#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

class Texture {
public:
	core::Result<> allocate(
		uint32_t width,
		uint32_t height,
		vk::PhysicalDevice physical_device,
		const vk::raii::Device &device,
		vk::Format format
	);

private:
	vk::raii::Image image;
	vk::raii::DeviceMemory memory;
	vk::raii::ImageView image_view;
};

} // namespace tramogi::graphics
