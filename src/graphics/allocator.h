#pragma once

#include "tramogi/core/errors.h"

namespace vk {
class PhysicalDevice;
class MemoryRequirements;
namespace raii {
class DeviceMemory;
} // namespace raii
} // namespace vk

namespace tramogi::graphics {

class Device;

enum class MemoryType {
	Host,
	Gpu
};

// TODO: Create an allocator and use offsets
[[nodiscard]] core::Result<vk::raii::DeviceMemory> allocate_memory(
	const Device &device,
	vk::MemoryRequirements memory_requirements,
	MemoryType memory_type
);

} // namespace tramogi::graphics
