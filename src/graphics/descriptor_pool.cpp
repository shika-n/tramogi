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

	// TODO: Don't hard code descriptorCount
	std::array pool_sizes {
		vk::DescriptorPoolSize {
			.type = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 200,
		},
		vk::DescriptorPoolSize {
			.type = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = 200,
		},
	};

	vk::DescriptorPoolCreateInfo pool_info {
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = max_set * 3,
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
