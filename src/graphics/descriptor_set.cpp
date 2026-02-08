#include "descriptor.h"
#include "device.h"
#include <memory>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

struct DescriptorSet::Impl {
	vk::raii::DescriptorSet descriptor_set = nullptr;
};

DescriptorSet::DescriptorSet(vk::raii::DescriptorSet &&descriptor_set)
	: impl(std::make_unique<Impl>()) {
	impl->descriptor_set = std::move(descriptor_set);
}
DescriptorSet::~DescriptorSet() = default;
DescriptorSet::DescriptorSet(DescriptorSet &&) = default;
DescriptorSet &DescriptorSet::operator=(DescriptorSet &&) = default;

const vk::raii::DescriptorSet &DescriptorSet::get_descriptor_set() {
	return impl->descriptor_set;
}

} // namespace tramogi::graphics
