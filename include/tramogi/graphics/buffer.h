#pragma once

#include "tramogi/core/errors.h"
#include <cstdint>
#include <memory>

namespace vk {
class PhysicalDevice;
namespace raii {
class Device;
class Buffer;
class CommandBuffer;
} // namespace raii
} // namespace vk

namespace tramogi::graphics {

enum class MemoryType;

class Buffer {
public:
	Buffer();
	virtual ~Buffer();

	virtual core::Result<> init(
		vk::PhysicalDevice physical_device,
		const vk::raii::Device &device,
		uint64_t size
	) = 0;

	void upload_data(const void *data);

	void map();
	void unmap();
	void *get_mapped_memory();
	vk::raii::Buffer &get_buffer();

	Buffer(const Buffer &) = delete;
	Buffer &operator=(const Buffer &) = delete;
	Buffer(Buffer &&);
	Buffer &operator=(Buffer &&);

protected:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

class StagingBuffer : public Buffer {
public:
	StagingBuffer() = default;
	~StagingBuffer() = default;
	core::Result<> init(
		vk::PhysicalDevice physical_device,
		const vk::raii::Device &device,
		uint64_t size
	);

	StagingBuffer(const StagingBuffer &) = delete;
	StagingBuffer &operator=(const StagingBuffer &) = delete;
	StagingBuffer(StagingBuffer &&) = default;
	StagingBuffer &operator=(StagingBuffer &&) = default;
};

class VertexBuffer : public Buffer {
public:
	VertexBuffer() = default;
	~VertexBuffer() = default;
	core::Result<> init(
		vk::PhysicalDevice physical_device,
		const vk::raii::Device &device,
		uint64_t size
	);

	VertexBuffer(const VertexBuffer &) = delete;
	VertexBuffer &operator=(const VertexBuffer &) = delete;
	VertexBuffer(VertexBuffer &&) = default;
	VertexBuffer &operator=(VertexBuffer &&) = default;
};

class IndexBuffer : public Buffer {
public:
	IndexBuffer() = default;
	~IndexBuffer() = default;
	core::Result<> init(
		vk::PhysicalDevice physical_device,
		const vk::raii::Device &device,
		uint64_t size
	);

	IndexBuffer(const IndexBuffer &) = delete;
	IndexBuffer &operator=(const IndexBuffer &) = delete;
	IndexBuffer(IndexBuffer &&) = default;
	IndexBuffer &operator=(IndexBuffer &&) = default;
};

class UniformBuffer : public Buffer {
public:
	UniformBuffer() = default;
	~UniformBuffer() = default;
	core::Result<> init(
		vk::PhysicalDevice physical_device,
		const vk::raii::Device &device,
		uint64_t size
	);

	UniformBuffer(const UniformBuffer &) = delete;
	UniformBuffer &operator=(const UniformBuffer &) = delete;
	UniformBuffer(UniformBuffer &&) = default;
	UniformBuffer &operator=(UniformBuffer &&) = default;
};

} // namespace tramogi::graphics
