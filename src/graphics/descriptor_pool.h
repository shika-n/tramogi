#pragma once

#include "tramogi/core/pointers.h"
#include <cstdint>

namespace vk::raii {
class DescriptorPool;
}

namespace tramogi::graphics {

class Device;

class DescriptorPool {
public:
	DescriptorPool(const Device &device, uint32_t max_set);
	~DescriptorPool();
	DescriptorPool(const DescriptorPool &) = delete;
	DescriptorPool &operator=(const DescriptorPool &) = delete;
	DescriptorPool(DescriptorPool &&);
	DescriptorPool &operator=(DescriptorPool &&);

	const vk::raii::DescriptorPool &get_descriptor_pool() const;

private:
	struct Impl;
	core::UniquePtr<Impl> impl;
};

} // namespace tramogi::graphics
