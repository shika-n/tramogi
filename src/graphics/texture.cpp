#include "tramogi/graphics/texture.h"
#include "allocator.h"
#include "device.h"
#include "tramogi/core/errors.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

using core::Error;
using core::Result;

struct Texture::Impl {
	vk::raii::Image image = nullptr;
	vk::raii::DeviceMemory memory = nullptr;
	vk::raii::ImageView image_view = nullptr;
};

Texture::Texture() : impl(std::make_unique<Impl>()) {}
Texture::~Texture() = default;
Texture::Texture(Texture &&) = default;
Texture &Texture::operator=(Texture &&) = default;

uint32_t calculate_mipmap_levels(uint32_t width, uint32_t height) {
	return std::max(std::log2(width), std::log2(height));
}

void transition_layout(
	[[maybe_unused]] const vk::raii::Image &image,
	[[maybe_unused]] vk::ImageLayout old_layout,
	[[maybe_unused]] vk::ImageLayout new_layout
) {
	// TODO: implementation
}

Result<void> Texture::init(const Device &device, uint32_t width, uint32_t height, bool mipmap) {
	vk::Format format = vk::Format::eR8G8B8A8Srgb;
	uint32_t mipmap_levels = 1;
	if (mipmap) {
		mipmap_levels = calculate_mipmap_levels(width, height);
	}

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

	impl->image = vk::raii::Image(device.get_device(), create_info);
	auto allocation_result =
		allocate_memory(device, impl->image.getMemoryRequirements(), MemoryType::Host);
	if (!allocation_result) {
		return Error(allocation_result.error());
	}

	impl->memory = std::move(allocation_result.value());
	impl->image.bindMemory(impl->memory, 0);

	impl->image_view = vk::raii::ImageView(
		device.get_device(),
		vk::ImageViewCreateInfo {
			.image = impl->image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = mipmap_levels,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		}
	);

	return {};
}

} // namespace tramogi::graphics
