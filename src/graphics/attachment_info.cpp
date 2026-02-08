#include "attachment_info.h"
#include "format.h"
#include "image.h"
#include "image_view.h"
#include "tramogi/core/types.h"
#include <array>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

using core::Optional;

vk::AttachmentLoadOp native(AttachmentLayout::LoadOperation operation) {
	switch (operation) {
	case AttachmentLayout::LoadOperation::Clear:
		return vk::AttachmentLoadOp::eClear;
	case AttachmentLayout::LoadOperation::Load:
		return vk::AttachmentLoadOp::eLoad;
	}

	std::unreachable();
}

struct AttachmentLayout::Impl {
	std::array<vk::RenderingAttachmentInfo, static_cast<uint8_t>(Type::Count)> attachment_infos;
};

AttachmentLayout::AttachmentLayout() : impl(std::make_unique<Impl>()) {
	for (uint8_t i = 0; i < MAX_COLOR_ATTACHMENTS; ++i) {
		impl->attachment_infos[i] = {
			.imageView = nullptr,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
		};
	}
	impl->attachment_infos[static_cast<uint8_t>(Type::Depth)] = {
		.imageView = nullptr,
		.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearDepthStencilValue(1.0f, 0),
	};
}
AttachmentLayout::~AttachmentLayout() = default;
AttachmentLayout::AttachmentLayout(AttachmentLayout &&) = default;
AttachmentLayout &AttachmentLayout::operator=(AttachmentLayout &&) = default;

void AttachmentLayout::add_attachment(Type type, Format format) {
	formats[static_cast<uint8_t>(type)] = format;

	if (static_cast<uint8_t>(type) < MAX_COLOR_ATTACHMENTS) {
		color_attachment_count++;
	} else if (type == Type::Depth) {
		is_depth_available = true;
	}
}

void AttachmentLayout::set_load_operation(Type type, LoadOperation operation) {
	impl->attachment_infos[static_cast<uint8_t>(type)].loadOp = native(operation);
}

std::span<const vk::RenderingAttachmentInfo> AttachmentLayout::get_color_infos(
	std::initializer_list<std::pair<Type, const ImageView *>> image_views
) const {
	for (auto pair : image_views) {
		impl->attachment_infos[static_cast<uint8_t>(pair.first)].imageView =
			pair.second->get_image_view();
	}
	return {impl->attachment_infos.begin(), color_attachment_count};
}

const vk::RenderingAttachmentInfo &AttachmentLayout::get_depth_info(
	const ImageView &image_view
) const {
	auto &attachment_info = impl->attachment_infos[static_cast<uint8_t>(Type::Depth)];
	attachment_info.imageView = image_view.get_image_view();
	return attachment_info;
}

std::span<const Format> AttachmentLayout::get_color_formats() const {
	return {formats.cbegin(), color_attachment_count};
}

Optional<Format> AttachmentLayout::get_depth_format() const {
	if (!is_depth_available) {
		return core::optional::none;
	}
	return formats[static_cast<uint8_t>(Type::Depth)];
}
} // namespace tramogi::graphics
