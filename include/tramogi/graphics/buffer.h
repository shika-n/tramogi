#pragma once

#include "tramogi/core/errors.h"
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

enum class MemoryType;

class Buffer {
public:
	[[nodiscard]] core::Result<> allocate(
		vk::PhysicalDevice physical_device,
		const vk::raii::Device &device,
		vk::DeviceSize,
		vk::BufferUsageFlags usage,
		MemoryType memory_type
	);

	void map();
	void unmap();
	void *get_mapped_memory() const {
		return mapped_memory;
	}

	vk::raii::Buffer &get_buffer() {
		return buffer;
	}

private:
	vk::raii::Buffer buffer = nullptr;
	vk::raii::DeviceMemory memory = nullptr;
	void *mapped_memory = nullptr;
};

} // namespace tramogi::graphics
