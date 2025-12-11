#include "tramogi/graphics/buffer.h"
#include "allocator.h"
#include "tramogi/core/errors.h"
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

using core::Result;

Result<> Buffer::allocate(
	vk::PhysicalDevice physical_device,
	const vk::raii::Device &device,
	vk::DeviceSize size,
	vk::BufferUsageFlags usage,
	MemoryType memory_type
) {
	vk::BufferCreateInfo create_info {
		.size = size,
		.usage = usage,
		.sharingMode = vk::SharingMode::eExclusive,
	};

	buffer = vk::raii::Buffer(device, create_info);

	auto allocation_result =
		allocate_memory(physical_device, device, buffer.getMemoryRequirements(), memory_type);
	if (!allocation_result) {
		return std::unexpected(allocation_result.error());
	}

	memory = std::move(allocation_result.value());
	buffer.bindMemory(memory, 0);

	return {};
}

void Buffer::map() {
	mapped_memory = memory.mapMemory(0, buffer.getMemoryRequirements().size);
}

void Buffer::unmap() {
	memory.unmapMemory();
	mapped_memory = nullptr;
}

} // namespace tramogi::graphics
