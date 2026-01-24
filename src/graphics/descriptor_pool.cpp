#include "descriptor_pool.h"
#include "device.h"
#include <cstdint>
#include <memory>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

struct DescriptorPool::Impl {
	vk::raii::DescriptorPool pool = nullptr;
};

DescriptorPool::DescriptorPool(const Device &device, uint32_t max_set)
	: impl(std::make_unique<Impl>()) {
	std::array pool_sizes {
		vk::DescriptorPoolSize {
			.type = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 2,
		},
		vk::DescriptorPoolSize {
			.type = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = 3,
		},
	};

	vk::DescriptorPoolCreateInfo pool_info {
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = max_set,
		.poolSizeCount = pool_sizes.size(),
		.pPoolSizes = pool_sizes.data(),
	};

	impl->pool = vk::raii::DescriptorPool(device.get_device(), pool_info);
};
DescriptorPool::~DescriptorPool() = default;
DescriptorPool::DescriptorPool(DescriptorPool &&) = default;
DescriptorPool &DescriptorPool::operator=(DescriptorPool &&) = default;

const vk::raii::DescriptorPool &DescriptorPool::get_descriptor_pool() const {
	return impl->pool;
}
} // namespace tramogi::graphics
