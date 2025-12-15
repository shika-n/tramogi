#include "tramogi/graphics/texture.h"
#include "allocator.h"
#include "tramogi/core/errors.h"
#include <cstdint>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

using core::Result;

Result<void> Texture::allocate(
	uint32_t width,
	uint32_t height,
	const vk::PhysicalDevice physical_device,
	const vk::raii::Device &device,
	vk::Format format
) {
	vk::ImageCreateInfo create_info {
		.imageType = vk::ImageType::e2D,
		.format = format,
		.extent = {width, height, 1},
		.mipLevels = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
				 vk::ImageUsageFlagBits::eSampled,
		.sharingMode = vk::SharingMode::eExclusive,
	};

	image = vk::raii::Image(device, create_info);
	auto allocation_result =
		allocate_memory(physical_device, device, image.getMemoryRequirements(), MemoryType::Host);
	if (!allocation_result) {
		return std::unexpected(allocation_result.error());
	}

	memory = std::move(allocation_result.value());
	image.bindMemory(memory, 0);

	return {};
}

} // namespace tramogi::graphics
