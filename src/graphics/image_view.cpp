#include "image_view.h"

#include "device.h"
#include "image.h"
#include "vulkan/vulkan.hpp"

#include <memory>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

struct ImageView::Impl {
	vk::raii::ImageView image_view = nullptr;
};

ImageView::ImageView(const Device &device, const Image &image) : impl(std::make_unique<Impl>()) {
	vk::ImageViewCreateInfo view_info {
		.image = image.get_image(),
		.viewType = vk::ImageViewType::e2D,
		.format = image.get_format(),
		.subresourceRange = {
			.aspectMask = image.get_aspect_flags(),
			.baseMipLevel = 0,
			.levelCount = image.get_mipmap_level(),
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
