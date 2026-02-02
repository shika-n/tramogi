#include "vertex_descriptor.h"
#include <cstdint>
#include <utility>
#include <vulkan/vulkan.hpp>

namespace tramogi::graphics {

VertexDescriptor::VertexDescriptor(Type type, uint32_t binding, uint32_t stride)
	: type(type), binding(binding), stride(stride) {}

void VertexDescriptor::add_attributes(const AttributeDescription &description) {
	descriptions.push_back(description);
}

vk::VertexInputRate to_vk_input_rate(VertexDescriptor::Type type) {
	switch (type) {
	case VertexDescriptor::Type::Vertex:
		return vk::VertexInputRate::eVertex;
	case VertexDescriptor::Type::Instance:
		return vk::VertexInputRate::eInstance;
	}

	assert(false && "Is vertex descriptor's type initialized?");
	std::unreachable();
}

vk::VertexInputBindingDescription VertexDescriptor::get_binding_description() const {
	return {
		.binding = binding,
		.stride = stride,
		.inputRate = to_vk_input_rate(type),
	};
}

std::vector<vk::VertexInputAttributeDescription> VertexDescriptor::
	get_attribute_description() const {
	std::vector<vk::VertexInputAttributeDescription> vk_descriptions;
	vk_descriptions.reserve(descriptions.size());

	for (const auto &desc : descriptions) {
		vk_descriptions.emplace_back(
			vk::VertexInputAttributeDescription {
				.location = desc.location,
				.binding = binding,
				.format = native(desc.format),
				.offset = desc.offset,
			}
		);
	}

	return vk_descriptions;
}

} // namespace tramogi::graphics
