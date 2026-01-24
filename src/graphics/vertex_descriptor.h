#pragma once

#include <cstdint>
#include <vector>

namespace vk {
class VertexInputAttributeDescription;
class VertexInputBindingDescription;
} // namespace vk

namespace tramogi::graphics {

struct AttributeDescription {
	enum class Format {
		Float2,
		Float3,
	};

	uint32_t location;
	Format format;
	uint32_t offset;
};

class VertexDescriptor {
public:
	enum class Type {
		Vertex,
		Instance
	};

	VertexDescriptor(Type type, uint32_t binding, uint32_t stride);

	void add_attributes(const AttributeDescription &description);

	vk::VertexInputBindingDescription get_binding_description() const;
	std::vector<vk::VertexInputAttributeDescription> get_attribute_description() const;

private:
	Type type;
	uint32_t binding;
	uint32_t stride;
	std::vector<AttributeDescription> descriptions;
};

} // namespace tramogi::graphics
