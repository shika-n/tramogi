#pragma once

#include "tramogi/core/errors.h"

namespace vk {
class PhysicalDevice;
class MemoryRequirements;
namespace raii {
class DeviceMemory;
class Device;
} // namespace raii
} // namespace vk

namespace tramogi::graphics {

enum class MemoryType {
	Host,
	Gpu
};

// TODO: Create an allocator and use offsets
[[nodiscard]] core::Result<vk::raii::DeviceMemory> allocate_memory(
	vk::PhysicalDevice physical_device,
	const vk::raii::Device &device,
	vk::MemoryRequirements memory_requirements,
	MemoryType memory_type
);

} // namespace tramogi::graphics
