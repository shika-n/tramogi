#include "image_view.h"

#include "device.h"
#include "format.h"
#include "image.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

struct ImageView::Impl {
	vk::raii::ImageView image_view = nullptr;
};

vk::ImageViewType native(ImageView::Type type) {
	switch (type) {
	case ImageView::Type::Texture:
		return vk::ImageViewType::e2D;
	case ImageView::Type::CubeMap:
		return vk::ImageViewType::eCube;
	}
	std::unreachable();
}

ImageView::ImageView(const Device &device, const Image &image) : impl(std::make_unique<Impl>()) {
	auto type = Type::Texture;
	uint32_t layer_count = 1;

	if (image.get_usage() == Image::Usage::CubeMap) {
		type = Type::CubeMap;
		layer_count = 6;
	}

	vk::ImageViewCreateInfo view_info {
		.image = image.get_image(),
		.viewType = native(type),
		.format = native(image.get_format()),
		.subresourceRange = {
			.aspectMask = image.get_aspect_flags(),
			.baseMipLevel = 0,
			.levelCount = image.get_mipmap_level(),
			.baseArrayLayer = 0,
			.layerCount = layer_count,
		}
	};
	impl->image_view = vk::raii::ImageView(device.get_device(), view_info);
}
ImageView::ImageView(
	const Device &device,
	const SwapchainImage &image,
	Format format,
	vk::ImageAspectFlags aspect_flags,
	uint32_t mipmap_level_count
)
	: impl(std::make_unique<Impl>()) {
	vk::ImageViewCreateInfo view_info {
		.image = image.get_image(),
		.viewType = native(Type::Texture),
		.format = native(format),
		.subresourceRange = {
			.aspectMask = aspect_flags,
			.baseMipLevel = 0,
			.levelCount = mipmap_level_count,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};
	impl->image_view = vk::raii::ImageView(device.get_device(), view_info);
}
ImageView::~ImageView() = default;
ImageView::ImageView(ImageView &&) = default;
ImageView &ImageView::operator=(ImageView &&) = default;

const vk::raii::ImageView &ImageView::get_image_view() const {
	return impl->image_view;
}

} // namespace tramogi::graphics
