#include "image.h"
#include "allocator.h"
#include "command_buffer.h"
#include "device.h"
#include "image_view.h"
#include "physical_device.h"
#include "tramogi/core/types.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

using core::Error;
using core::Result;

struct Image::Impl {
	vk::raii::Image image = nullptr;
	vk::raii::DeviceMemory memory = nullptr;

	vk::Format format;

	void transition_image_layout(
		const CommandBuffer &cmd,
		vk::ImageLayout old_layout,
		vk::ImageLayout new_layout,
		vk::AccessFlags2 src_access_mask,
		vk::AccessFlags2 dst_access_mask,
		vk::PipelineStageFlags2 src_stage_mask,
		vk::PipelineStageFlags2 dst_stage_mask,
		vk::ImageAspectFlags aspect_flags
	) {
		vk::ImageMemoryBarrier2 barrier = {
			.srcStageMask = src_stage_mask,
			.srcAccessMask = src_access_mask,
			.dstStageMask = dst_stage_mask,
			.dstAccessMask = dst_access_mask,
			.oldLayout = old_layout,
			.newLayout = new_layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = {
				.aspectMask = aspect_flags,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};

		vk::DependencyInfo dependency_info {
			.dependencyFlags = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier,
		};

		cmd.get_command_buffer().pipelineBarrier2(dependency_info);
	}
};

uint32_t calculate_mipmap_levels(uint32_t width, uint32_t height) {
	return std::max(std::log2(width), std::log2(height));
}

Image::Image() : impl(std::make_unique<Impl>()) {}
Image::Image(
	const PhysicalDevice &,
	const Device &device,
	uint32_t width,
	uint32_t height,
	bool mipmap
)
	: impl(std::make_unique<Impl>()) {
	impl->format = vk::Format::eR8G8B8A8Srgb;
	if (mipmap) {
		mipmap_level_count = calculate_mipmap_levels(width, height);
	}

	vk::ImageCreateInfo create_info {
		.imageType = vk::ImageType::e2D,
		.format = impl->format,
		.extent = {width, height, 1},
		.mipLevels = mipmap_level_count,
		.arrayLayers = 1,
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
		throw std::runtime_error(allocation_result.error());
	}

	impl->memory = std::move(allocation_result.value());
	impl->image.bindMemory(impl->memory, 0);
}

Image::~Image() = default;
Image::Image(Image &&) = default;
Image &Image::operator=(Image &&) = default;

void Image::as_color_target(const CommandBuffer &cmd) {
	impl->transition_image_layout(
		cmd,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		{},
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::ImageAspectFlagBits::eColor
	);
}

void Image::as_present_source(const CommandBuffer &cmd) {
	impl->transition_image_layout(
		cmd,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::ePresentSrcKHR,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		{},
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eBottomOfPipe,
		vk::ImageAspectFlagBits::eColor
	);
}

const vk::raii::Image &Image::get_image() const {
	return impl->image;
}

vk::Format Image::get_format() const {
	return impl->format;
}

vk::ImageAspectFlags Image::get_aspect_flags() const {
	return vk::ImageAspectFlagBits::eColor;
}

DepthImage::DepthImage(
	const PhysicalDevice &physical_device,
	const Device &device,
	uint32_t width,
	uint32_t height,
	bool mipmap
) {
	Result<vk::Format> format = physical_device.get_depth_format();
	if (!format) {
		throw std::runtime_error(format.error());
	}
	impl->format = format.value();
	if (mipmap) {
		mipmap_level_count = calculate_mipmap_levels(width, height);
	}

	vk::ImageCreateInfo create_info {
		.imageType = vk::ImageType::e2D,
		.format = impl->format,
		.extent = {width, height, 1},
		.mipLevels = mipmap_level_count,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
		.sharingMode = vk::SharingMode::eExclusive,
	};

	impl->image = vk::raii::Image(device.get_device(), create_info);
	auto allocation_result =
		allocate_memory(device, impl->image.getMemoryRequirements(), MemoryType::Gpu);
	if (!allocation_result) {
		throw std::runtime_error(allocation_result.error());
	}

	impl->memory = std::move(allocation_result.value());
	impl->image.bindMemory(impl->memory, 0);
}

void DepthImage::as_depth_target(const CommandBuffer &cmd) {
	impl->transition_image_layout(
		cmd,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eDepthAttachmentOptimal,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests |
			vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests |
			vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::ImageAspectFlagBits::eDepth
	);
}

vk::ImageAspectFlags DepthImage::get_aspect_flags() const {
	return vk::ImageAspectFlagBits::eDepth;
}

} // namespace tramogi::graphics
