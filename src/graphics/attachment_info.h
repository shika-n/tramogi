#pragma once

#include "format.h"
#include "tramogi/core/types.h"
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>

namespace vk {
class RenderingAttachmentInfo;
}

namespace tramogi::graphics {

class ImageView;

class AttachmentLayout {
public:
	enum class Type : uint8_t {
		Color0,
		Color1,
		Color2,
		Color3,
		Depth,
		Count
	};
	constexpr static uint8_t MAX_COLOR_ATTACHMENTS = static_cast<uint8_t>(Type::Color3) + 1;

	AttachmentLayout();
	~AttachmentLayout();
	AttachmentLayout(const AttachmentLayout &) = delete;
	AttachmentLayout &operator=(const AttachmentLayout &) = delete;
	AttachmentLayout(AttachmentLayout &&);
	AttachmentLayout &operator=(AttachmentLayout &&);

	void add_attachment(Type type, Format format);

	bool is_depth_exists() const;

	std::span<const vk::RenderingAttachmentInfo> get_color_infos(
		std::initializer_list<std::pair<Type, const ImageView *>> image_views
	) const;
	const vk::RenderingAttachmentInfo &get_depth_info(const ImageView &image_view) const;
	std::span<const Format> get_color_formats() const;
	core::Optional<Format> get_depth_format() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
	std::array<Format, static_cast<uint8_t>(Type::Count)> formats;
	uint8_t color_attachment_count = 0;
	bool is_depth_available = false;
};

} // namespace tramogi::graphics
