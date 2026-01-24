#pragma once

#include <cstdint>
#include <memory>
#include <span>

namespace vk {
enum class DescriptorType;
namespace raii {
class DescriptorSet;
class DescriptorSetLayout;
} // namespace raii
} // namespace vk

namespace tramogi::graphics {

class Device;

struct DescriptorLayoutBinding {
	enum class Stage {
		Vertex,
		Fragment
	};

	enum class Type {
		UniformbBuffer,
		CombinedSampler
	};

	uint32_t location = 1;
	uint32_t count = 1;
	Stage stage;
	Type type;
};

class DescriptorLayout {
public:
	DescriptorLayout(const Device &device, const std::span<const DescriptorLayoutBinding> layouts);
	~DescriptorLayout();
	DescriptorLayout(const DescriptorLayout &) = delete;
	DescriptorLayout &operator=(const DescriptorLayout &) = delete;
	DescriptorLayout(DescriptorLayout &&);
	DescriptorLayout &operator=(DescriptorLayout &&);

	const vk::raii::DescriptorSetLayout &get_layout() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

class DescriptorSet {
public:
	DescriptorSet(vk::raii::DescriptorSet &&descriptor_set);
	~DescriptorSet();
	DescriptorSet(const DescriptorSet &) = delete;
	DescriptorSet &operator=(const DescriptorSet &) = delete;
	DescriptorSet(DescriptorSet &&);
	DescriptorSet &operator=(DescriptorSet &&);

	const vk::raii::DescriptorSet &get_descriptor_set();

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace tramogi::graphics
