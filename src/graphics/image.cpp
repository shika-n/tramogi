#include "image.h"
#include "allocator.h"
#include "command_buffer.h"
#include "device.h"
#include "format.h"
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

	Format format;
};

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

Image::Image() : impl(std::make_unique<Impl>()) {}
Image::Image(
	const PhysicalDevice &,
	const Device &device,
	uint32_t width,
	uint32_t height,
	Format format,
	bool mipmap
)
	: impl(std::make_unique<Impl>()) {
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
		.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
		// .usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
		// 		 vk::ImageUsageFlagBits::eSampled,
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

void Image::as_color_target(const CommandBuffer &cmd) const {
	transition_image_layout(
		cmd,
		impl->image,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::ImageAspectFlagBits::eColor
	);
}

void Image::as_sampled(const CommandBuffer &cmd) const {
	transition_image_layout(
		cmd,
		impl->image,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::AccessFlagBits2::eShaderRead,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::ImageAspectFlagBits::eColor
	);
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

	vk::ImageCreateInfo create_info {
		.imageType = vk::ImageType::e2D,
		.format = native(impl->format),
		.extent = {width, height, 1},
		.mipLevels = mipmap_level_count,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
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
		vk::ImageAspectFlagBits::eColor
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
		vk::ImageAspectFlagBits::eColor
	);
}

vk::Image SwapchainImage::get_image() const {
	return impl->image;
}

} // namespace tramogi::graphics

/*

	uint32_t mip_levels = 0;
	vk::raii::Image texture_image = nullptr;
	vk::raii::DeviceMemory texture_memory = nullptr;
	vk::raii::ImageView texture_image_view = nullptr;
	vk::raii::Sampler texture_sampler = nullptr;

	void create_texture_image() {
		ImageData image_data;
		if (!image_data.load_from_file(TEXTURE_PATH.c_str())) {
			// TODO: handle missing texture without throwing
			throw std::runtime_error("Failed to load texture image");
		}

		int texture_width = image_data.get_width();
		int texture_height = image_data.get_height();
		mip_levels = image_data.get_mip_levels();
		vk::DeviceSize image_size = image_data.get_size();

		StagingBuffer staging_buffer(device, image_size);

		staging_buffer.map();
		staging_buffer.upload_data(image_data.get_data());
		staging_buffer.unmap();

		create_image(
			texture_width,
			texture_height,
			mip_levels,
			vk::Format::eR8G8B8A8Srgb,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
				vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			texture_image,
			texture_memory
		);

		transition_image_layout(
			texture_image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			mip_levels
		);
		copy_buffer_to_image(
			staging_buffer.get_buffer(),
			texture_image,
			texture_width,
			texture_height
		);

		generate_mipmaps(texture_image, texture_width, texture_height, mip_levels);
	}

	void generate_mipmaps(
		vk::Image image,
		int32_t texture_width,
		int32_t texture_height,
		uint32_t mip_levels
	) {
		CommandBuffer cmd = device.allocate_command_buffer();
		cmd.begin_onetime();

		vk::ImageMemoryBarrier barrier {
			.srcAccessMask = vk::AccessFlagBits::eTransferWrite,
			.dstAccessMask = vk::AccessFlagBits::eTransferRead,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eTransferSrcOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = image,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		int32_t mip_width = texture_width;
		int32_t mip_height = texture_height;

		for (uint32_t i = 1; i < mip_levels; ++i) {
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
			dst_offsets[1] = vk::Offset3D(
				mip_width > 1 ? mip_width / 2 : 1,
				mip_height > 1 ? mip_height / 2 : 1,
				1
			);

			vk::ImageBlit blit = {
				.srcSubresource =
					{
						.aspectMask = vk::ImageAspectFlagBits::eColor,
						.mipLevel = i - 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				.srcOffsets = offsets,
				.dstSubresource =
					{
						.aspectMask = vk::ImageAspectFlagBits::eColor,
						.mipLevel = i,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				.dstOffsets = dst_offsets,
			};

			cmd.get_command_buffer().blitImage(
				image,
				vk::ImageLayout::eTransferSrcOptimal,
				image,
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
		barrier.subresourceRange.baseMipLevel = mip_levels - 1;
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

		cmd.end();
		device.submit(cmd);
	}

	void create_texture_image_view() {
		texture_image_view = create_image_view(
			texture_image,
			vk::Format::eR8G8B8A8Srgb,
			vk::ImageAspectFlagBits::eColor,
			mip_levels
		);
	}

	void create_texture_sampler() {
		vk::PhysicalDeviceProperties properties =
			physical_device.get_physical_device().getProperties();
		vk::SamplerCreateInfo sampler_info {
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eLinear,
			.addressModeU = vk::SamplerAddressMode::eRepeat,
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.addressModeW = vk::SamplerAddressMode::eRepeat,
			.mipLodBias = 0.0f,
			.anisotropyEnable = vk::True,
			.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
			.compareEnable = vk::False,
			.compareOp = vk::CompareOp::eAlways,
		};
		texture_sampler = vk::raii::Sampler(device.get_device(), sampler_info);
	}

	void create_image(
		uint32_t width,
		uint32_t height,
		uint32_t mip_levels,
		vk::Format format,
		vk::ImageTiling tiling,
		vk::ImageUsageFlags usage,
		vk::MemoryPropertyFlags properties,
		vk::raii::Image &image,
		vk::raii::DeviceMemory &image_memory
	) {
		vk::ImageCreateInfo image_info {
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = {width, height, 1},
			.mipLevels = mip_levels,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = tiling,
			.usage = usage,
			.sharingMode = vk::SharingMode::eExclusive,
		};

		image = vk::raii::Image(device.get_device(), image_info);

		vk::MemoryRequirements memory_requirements = image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocate_info {
			.allocationSize = memory_requirements.size,
			.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties),
		};

		image_memory = vk::raii::DeviceMemory(device.get_device(), allocate_info);
		image.bindMemory(image_memory, 0);
	}

	void copy_buffer_to_image(
		const vk::raii::Buffer &buffer,
		vk::raii::Image &image,
		uint32_t width,
		uint32_t height
	) {
		CommandBuffer cmd = device.allocate_command_buffer();
		cmd.begin_onetime();

		vk::BufferImageCopy region {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
			.imageOffset = {0, 0, 0},
			.imageExtent = {width, height, 1},
		};

		cmd.get_command_buffer()
			.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

		cmd.end();
		device.submit(cmd);
	}

	uint32_t find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) {
		auto memory_properties = physical_device.get_memory_properties();
		for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
			if (type_filter & (1 << i) &&
				(memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("Failed to find suitable memory type");
	}

	void transition_image_layout(
		const vk::raii::Image &image,
		vk::ImageLayout old_layout,
		vk::ImageLayout new_layout,
		uint32_t mip_levels
	) {
		CommandBuffer cmd = device.allocate_command_buffer();
		cmd.begin_onetime();

		vk::PipelineStageFlags source_stage;
		vk::PipelineStageFlags destination_stage;

		vk::ImageMemoryBarrier barrier {
			.oldLayout = old_layout,
			.newLayout = new_layout,
			.image = image,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = mip_levels,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};

		if (old_layout == vk::ImageLayout::eUndefined &&
			new_layout == vk::ImageLayout::eTransferDstOptimal) {
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
			destination_stage = vk::PipelineStageFlagBits::eTransfer;
		} else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
				   new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			source_stage = vk::PipelineStageFlagBits::eTransfer;
			destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
		} else {
			throw std::invalid_argument("Unsupported layout transition");
		}

		cmd.get_command_buffer()
			.pipelineBarrier(source_stage, destination_stage, {}, {}, nullptr, barrier);

		cmd.end();
		device.submit(cmd);
	}

	vk::raii::ImageView create_image_view(
		vk::Image image,
		vk::Format format,
		vk::ImageAspectFlags aspect_flags,
		uint32_t mip_levels
	) {
		vk::ImageViewCreateInfo view_info {
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = {
				.aspectMask = aspect_flags,
				.baseMipLevel = 0,
				.levelCount = mip_levels,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};
		return vk::raii::ImageView(device.get_device(), view_info);
	}

 */
