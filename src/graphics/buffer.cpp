#include "tramogi/graphics/buffer.h"
#include "../logging.h"
#include "allocator.h"
#include "tramogi/core/errors.h"
#include <cassert>
#include <cstdint>
#include <memory>
#include <string.h>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

using core::Result;

struct Buffer::Impl {
	vk::raii::Buffer buffer = nullptr;
	vk::raii::DeviceMemory memory = nullptr;
	void *mapped_memory = nullptr;

	uint64_t buffer_size = 0;
	MemoryType memory_type;
};

void Buffer::upload_data(const void *data) {
	memcpy(get_mapped_memory(), data, impl->buffer_size);
}

void Buffer::map() {
	assert(
		impl->memory_type == MemoryType::Host && "Should only map memory that is visible to host"
	);
	impl->mapped_memory = impl->memory.mapMemory(0, impl->buffer.getMemoryRequirements().size);
}

void Buffer::unmap() {
	impl->memory.unmapMemory();
	impl->mapped_memory = nullptr;
}

void *Buffer::get_mapped_memory() {
	assert(impl->mapped_memory != nullptr && "Memory hasn't been mapped yet");
	return impl->mapped_memory;
}

vk::raii::Buffer &Buffer::get_buffer() {
	return impl->buffer;
}

Buffer::Buffer() : impl(std::make_unique<Impl>()) {};
Buffer::~Buffer() {
	DLOG("Buffer destroyed");
};
Buffer::Buffer(Buffer &&) = default;
Buffer &Buffer::operator=(Buffer &&) = default;

Result<> StagingBuffer::init(
	vk::PhysicalDevice physical_device,
	const vk::raii::Device &device,
	uint64_t size
) {
	vk::BufferCreateInfo create_info {
		.size = size,
		.usage = vk::BufferUsageFlagBits::eTransferSrc,
		.sharingMode = vk::SharingMode::eExclusive,
	};

	impl->buffer = vk::raii::Buffer(device, create_info);
	impl->buffer_size = size;
	impl->memory_type = MemoryType::Host;

	auto allocation_result = allocate_memory(
		physical_device,
		device,
		impl->buffer.getMemoryRequirements(),
		impl->memory_type
	);
	if (!allocation_result) {
		return std::unexpected(allocation_result.error());
	}

	impl->memory = std::move(allocation_result.value());
	impl->buffer.bindMemory(impl->memory, 0);

	return {};
}

Result<> VertexBuffer::init(
	vk::PhysicalDevice physical_device,
	const vk::raii::Device &device,
	uint64_t size
) {
	vk::BufferCreateInfo create_info {
		.size = size,
		.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		.sharingMode = vk::SharingMode::eExclusive,
	};

	impl->buffer = vk::raii::Buffer(device, create_info);
	impl->buffer_size = size;
	impl->memory_type = MemoryType::Gpu;

	auto allocation_result = allocate_memory(
		physical_device,
		device,
		impl->buffer.getMemoryRequirements(),
		impl->memory_type
	);
	if (!allocation_result) {
		return std::unexpected(allocation_result.error());
	}

	impl->memory = std::move(allocation_result.value());
	impl->buffer.bindMemory(impl->memory, 0);

	return {};
}

Result<> IndexBuffer::init(
	vk::PhysicalDevice physical_device,
	const vk::raii::Device &device,
	uint64_t size
) {
	vk::BufferCreateInfo create_info {
		.size = size,
		.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		.sharingMode = vk::SharingMode::eExclusive,
	};

	impl->buffer = vk::raii::Buffer(device, create_info);
	impl->buffer_size = size;
	impl->memory_type = MemoryType::Gpu;

	auto allocation_result = allocate_memory(
		physical_device,
		device,
		impl->buffer.getMemoryRequirements(),
		impl->memory_type
	);
	if (!allocation_result) {
		return std::unexpected(allocation_result.error());
	}

	impl->memory = std::move(allocation_result.value());
	impl->buffer.bindMemory(impl->memory, 0);

	return {};
}

Result<> UniformBuffer::init(
	vk::PhysicalDevice physical_device,
	const vk::raii::Device &device,
	uint64_t size
) {
	vk::BufferCreateInfo create_info {
		.size = size,
		.usage = vk::BufferUsageFlagBits::eUniformBuffer,
		.sharingMode = vk::SharingMode::eExclusive,
	};

	impl->buffer = vk::raii::Buffer(device, create_info);
	impl->buffer_size = size;
	impl->memory_type = MemoryType::Host;

	auto allocation_result = allocate_memory(
		physical_device,
		device,
		impl->buffer.getMemoryRequirements(),
		impl->memory_type
	);
	if (!allocation_result) {
		return std::unexpected(allocation_result.error());
	}

	impl->memory = std::move(allocation_result.value());
	impl->buffer.bindMemory(impl->memory, 0);

	return {};
}

} // namespace tramogi::graphics
