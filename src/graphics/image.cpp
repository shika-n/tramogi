#include "image.h"
#include "allocator.h"
#include "command_buffer.h"
#include "device.h"
#include "format.h"
#include "image_view.h"
#include "physical_device.h"
#include "tramogi/core/types.h"
#include "vulkan/vulkan.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

using core::Error;
using core::Result;

struct Image::Impl {
	vk::raii::Image image = nullptr;
	vk::raii::DeviceMemory memory = nullptr;

	vk::ImageLayout current_layout = vk::ImageLayout::eUndefined;
	vk::AccessFlags2 current_access_mask = vk::AccessFlagBits2::eNone;
	vk::PipelineStageFlags2 current_stage_mask = vk::PipelineStageFlagBits2::eNone;

	Format format;
};

vk::ImageUsageFlags native(Image::Usage usage) {
	switch (usage) {
	case Image::Usage::SampledColorTarget:
		return vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
	case Image::Usage::SampledDepth:
		return vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
	case Image::Usage::CubeMap:
	case Image::Usage::Texture:
		return vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
			   vk::ImageUsageFlagBits::eSampled;
	}
	std::unreachable();
}

uint32_t calculate_mipmap_levels(uint32_t width, uint32_t height) {
	return std::max(std::log2(width), std::log2(height));
}

void transition_image_layout(
	const CommandBuffer &cmd,
	vk::Image image,
	vk::ImageLayout old_layout,
	vk::ImageLayout new_layout,
	vk::AccessFlags2 src_access_mask,
	vk::AccessFlags2 dst_access_mask,
	vk::PipelineStageFlags2 src_stage_mask,
	vk::PipelineStageFlags2 dst_stage_mask,
	vk::ImageAspectFlags aspect_flags,
	uint32_t layer_count
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
			.layerCount = layer_count,
		}
	};

	vk::DependencyInfo dependency_info {
		.dependencyFlags = {},
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier,
	};

	cmd.get_command_buffer().pipelineBarrier2(dependency_info);
}

Image::Image() : impl(std::make_unique<Impl>()) {}
Image::Image(
	const PhysicalDevice &,
	const Device &device,
	uint32_t width,
	uint32_t height,
	Format format,
	Usage usage,
	bool mipmap
)
	: impl(std::make_unique<Impl>()), width(width), height(height), usage(Usage::Texture) {
	impl->format = format;
	if (mipmap) {
		mipmap_level_count = calculate_mipmap_levels(width, height);
	}

	vk::ImageCreateInfo create_info {
		.imageType = vk::ImageType::e2D,
		.format = native(impl->format),
		.extent = {width, height, 1},
		.mipLevels = mipmap_level_count,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = native(usage),
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

Image::~Image() = default;
Image::Image(Image &&) = default;
Image &Image::operator=(Image &&) = default;

void Image::generate_mipmap(const CommandBuffer &cmd) {
	vk::ImageMemoryBarrier barrier {
		.srcAccessMask = vk::AccessFlagBits::eTransferWrite,
		.dstAccessMask = vk::AccessFlagBits::eTransferRead,
		.oldLayout = vk::ImageLayout::eTransferDstOptimal,
		.newLayout = vk::ImageLayout::eTransferSrcOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = impl->image,
		.subresourceRange = {
			.aspectMask = get_aspect_flags(),
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	int32_t mip_width = width;
	int32_t mip_height = height;

	for (uint32_t i = 1; i < mipmap_level_count; ++i) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

		cmd.get_command_buffer().pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eTransfer,
			{},
			{},
			{},
			barrier
		);

		vk::ArrayWrapper1D<vk::Offset3D, 2> offsets;
		vk::ArrayWrapper1D<vk::Offset3D, 2> dst_offsets;
		offsets[0] = vk::Offset3D(0, 0, 0);
		offsets[1] = vk::Offset3D(mip_width, mip_height, 1);
		dst_offsets[0] = vk::Offset3D(0, 0, 0);
		dst_offsets[1] =
			vk::Offset3D(mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1);

		vk::ImageBlit blit = {
			.srcSubresource =
				{
					.aspectMask = get_aspect_flags(),
					.mipLevel = i - 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			.srcOffsets = offsets,
			.dstSubresource =
				{
					.aspectMask = get_aspect_flags(),
					.mipLevel = i,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			.dstOffsets = dst_offsets,
		};

		cmd.get_command_buffer().blitImage(
			impl->image,
			vk::ImageLayout::eTransferSrcOptimal,
			impl->image,
			vk::ImageLayout::eTransferDstOptimal,
			blit,
			vk::Filter::eLinear
		);

		barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		cmd.get_command_buffer().pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader,
			{},
			{},
			{},
			barrier
		);

		if (mip_width > 1) {
			mip_width /= 2;
		}
		if (mip_height > 1) {
			mip_height /= 2;
		}
	}
	barrier.subresourceRange.baseMipLevel = mipmap_level_count - 1;
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

	cmd.get_command_buffer().pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eFragmentShader,
		{},
		{},
		{},
		barrier
	);
}

void Image::as_color_target(const CommandBuffer &cmd) const {
	uint32_t layer_count = 1;
	if (usage == Usage::CubeMap) {
		layer_count = 6;
	}

	transition_image_layout(
		cmd,
		impl->image,
		impl->current_layout,
		vk::ImageLayout::eColorAttachmentOptimal,
		impl->current_access_mask,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		impl->current_stage_mask,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		get_aspect_flags(),
		layer_count
	);
	impl->current_layout = vk::ImageLayout::eColorAttachmentOptimal;
	impl->current_access_mask = vk::AccessFlagBits2::eColorAttachmentWrite;
	impl->current_stage_mask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
}

void Image::as_transfer_src(const CommandBuffer &cmd) const {
	uint32_t layer_count = 1;
	if (usage == Usage::CubeMap) {
		layer_count = 6;
	}

	transition_image_layout(
		cmd,
		impl->image,
		impl->current_layout,
		vk::ImageLayout::eTransferSrcOptimal,
		impl->current_access_mask,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		impl->current_stage_mask,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		get_aspect_flags(),
		layer_count
	);
	impl->current_layout = vk::ImageLayout::eTransferSrcOptimal;
	impl->current_access_mask = vk::AccessFlagBits2::eColorAttachmentWrite;
	impl->current_stage_mask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
}

void Image::as_transfer_dst(const CommandBuffer &cmd) const {
	uint32_t layer_count = 1;
	if (usage == Usage::CubeMap) {
		layer_count = 6;
	}

	transition_image_layout(
		cmd,
		impl->image,
		impl->current_layout,
		vk::ImageLayout::eTransferDstOptimal,
		impl->current_access_mask,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		impl->current_stage_mask,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		get_aspect_flags(),
		layer_count
	);
	impl->current_layout = vk::ImageLayout::eTransferDstOptimal;
	impl->current_access_mask = vk::AccessFlagBits2::eColorAttachmentWrite;
	impl->current_stage_mask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
}

void Image::as_sampled(const CommandBuffer &cmd) const {
	uint32_t layer_count = 1;
	if (usage == Usage::CubeMap) {
		layer_count = 6;
	}

	transition_image_layout(
		cmd,
		impl->image,
		impl->current_layout,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		impl->current_access_mask,
		vk::AccessFlagBits2::eShaderRead,
		impl->current_stage_mask,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		get_aspect_flags(),
		layer_count
	);
	impl->current_layout = vk::ImageLayout::eShaderReadOnlyOptimal;
	impl->current_access_mask = vk::AccessFlagBits2::eShaderRead;
	impl->current_stage_mask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
}

const vk::raii::Image &Image::get_image() const {
	return impl->image;
}

Format Image::get_format() const {
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
	Result<Format> format = physical_device.get_depth_format();
	if (!format) {
		throw std::runtime_error(format.error());
	}
	impl->format = format.value();
	if (mipmap) {
		mipmap_level_count = calculate_mipmap_levels(width, height);
	}

	usage = Usage::SampledDepth;

	vk::ImageCreateInfo create_info {
		.imageType = vk::ImageType::e2D,
		.format = native(impl->format),
		.extent = {width, height, 1},
		.mipLevels = mipmap_level_count,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = native(usage),
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

void DepthImage::as_depth_target(const CommandBuffer &cmd) const {
	transition_image_layout(
		cmd,
		impl->image,
		impl->current_layout,
		vk::ImageLayout::eDepthAttachmentOptimal,
		impl->current_access_mask,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		impl->current_stage_mask,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests |
			vk::PipelineStageFlagBits2::eLateFragmentTests,
		get_aspect_flags(),
		1
	);
	impl->current_layout = vk::ImageLayout::eDepthAttachmentOptimal;
	impl->current_access_mask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
	impl->current_stage_mask = vk::PipelineStageFlagBits2::eEarlyFragmentTests |
							   vk::PipelineStageFlagBits2::eLateFragmentTests;
}

vk::ImageAspectFlags DepthImage::get_aspect_flags() const {
	return vk::ImageAspectFlagBits::eDepth;
}

struct SwapchainImage::Impl {
	vk::Image image;
};

SwapchainImage::SwapchainImage(vk::Image image) : impl(std::make_unique<Impl>()) {
	impl->image = image;
}
SwapchainImage::~SwapchainImage() = default;
SwapchainImage::SwapchainImage(SwapchainImage &&) = default;
SwapchainImage &SwapchainImage::operator=(SwapchainImage &&) = default;

void SwapchainImage::as_attachment(const CommandBuffer &cmd) const {
	transition_image_layout(
		cmd,
		impl->image,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		{},
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::ImageAspectFlagBits::eColor,
		1
	);
}
void SwapchainImage::as_present_source(const CommandBuffer &cmd) const {
	transition_image_layout(
		cmd,
		impl->image,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::ePresentSrcKHR,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		{},
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eBottomOfPipe,
		vk::ImageAspectFlagBits::eColor,
		1
	);
}

vk::Image SwapchainImage::get_image() const {
	return impl->image;
}

CubeMapImage::CubeMapImage(
	const PhysicalDevice &,
	const Device &device,
	uint32_t width,
	uint32_t height,
	Format format,
	bool mipmap
) {
	impl->format = format;
	if (mipmap) {
		mipmap_level_count = calculate_mipmap_levels(width, height);
	}

	usage = Usage::CubeMap;

	vk::ImageCreateInfo create_info {
		.imageType = vk::ImageType::e2D,
		.format = native(impl->format),
		.extent = {width, height, 1},
		.mipLevels = mipmap_level_count,
		.arrayLayers = 6,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = native(usage),
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

CubeMapImage::~CubeMapImage() = default;
CubeMapImage::CubeMapImage(CubeMapImage &&) = default;
CubeMapImage &CubeMapImage::operator=(CubeMapImage &&) = default;

} // namespace tramogi::graphics
