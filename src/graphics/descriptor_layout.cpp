#include "descriptor.h"
#include "device.h"
#include <cstdint>
#include <memory>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

struct DescriptorLayout::Impl {
	vk::raii::DescriptorSetLayout layout = nullptr;
};

vk::DescriptorType to_vk(DescriptorLayoutBinding::Type type) {
	switch (type) {
	case DescriptorLayoutBinding::Type::CombinedSampler:
		return vk::DescriptorType::eCombinedImageSampler;
	case DescriptorLayoutBinding::Type::UniformbBuffer:
		return vk::DescriptorType::eUniformBuffer;
	}
	throw std::runtime_error("Unknown type for DescriptorLayoutBinding::Type");
}

vk::ShaderStageFlags to_vk(DescriptorLayoutBinding::Stage stage) {
	switch (stage) {
	case DescriptorLayoutBinding::Stage::Fragment:
		return vk::ShaderStageFlagBits::eFragment;
	case DescriptorLayoutBinding::Stage::Vertex:
		return vk::ShaderStageFlagBits::eVertex;
	}
	throw std::runtime_error("Unknown stage for DescriptorLayoutBinding::Stage");
}

DescriptorLayout::DescriptorLayout(
	const Device &device,
	const std::span<const DescriptorLayoutBinding> layouts
)
	: impl(std::make_unique<Impl>()) {
	std::vector<vk::DescriptorSetLayoutBinding> vk_layouts;
	for (const auto &layout : layouts) {
		vk_layouts.emplace_back(
			vk::DescriptorSetLayoutBinding {
				.binding = layout.location,
				.descriptorType = to_vk(layout.type),
				.descriptorCount = layout.count,
				.stageFlags = to_vk(layout.stage),
				.pImmutableSamplers = nullptr,
			}
		);
	}

	vk::DescriptorSetLayoutCreateInfo layout_info {
		.bindingCount = static_cast<uint32_t>(vk_layouts.size()),
		.pBindings = vk_layouts.data(),
	};

	impl->layout = vk::raii::DescriptorSetLayout(device.get_device(), layout_info);
};
DescriptorLayout::~DescriptorLayout() = default;
DescriptorLayout::DescriptorLayout(DescriptorLayout &&) = default;
DescriptorLayout &DescriptorLayout::operator=(DescriptorLayout &&) = default;

const vk::raii::DescriptorSetLayout &DescriptorLayout::get_layout() const {
	return impl->layout;
}

} // namespace tramogi::graphics
