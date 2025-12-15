#include "allocator.h"
#include "tramogi/core/errors.h"
#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

using core::Result;

Result<uint32_t> find_memory_type(
	vk::PhysicalDevice physical_device,
	uint32_t type_filter,
	vk::MemoryPropertyFlags properties
) {
	auto memory_properties = physical_device.getMemoryProperties();
	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
		if (type_filter & (1 << i) &&
			(memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	return std::unexpected("No suitable memory type");
}

core::Result<vk::raii::DeviceMemory> allocate_memory(
	vk::PhysicalDevice physical_device,
	const vk::raii::Device &device,
	vk::MemoryRequirements memory_requirements,
	MemoryType memory_type
) {
	auto properties = vk::MemoryPropertyFlagBits::eHostVisible |
					  vk::MemoryPropertyFlagBits::eHostCoherent;
	if (memory_type == MemoryType::Gpu) {
		properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
	}

	auto memory_index =
		find_memory_type(physical_device, memory_requirements.memoryTypeBits, properties);
	if (!memory_index) {
		return std::unexpected(memory_index.error());
	}

	vk::MemoryAllocateInfo allocate_info {
		.allocationSize = memory_requirements.size,
		.memoryTypeIndex = memory_index.value()
	};

	return vk::raii::DeviceMemory(device, allocate_info);
}

} // namespace tramogi::graphics
