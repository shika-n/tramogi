#pragma once

#include "tramogi/core/pointers.h"
#include <cstdint>

namespace vk {
namespace raii {
class Buffer;
class CommandBuffer;
} // namespace raii
} // namespace vk

namespace tramogi::graphics {

enum class MemoryType;
class Device;

class Buffer {
public:
	Buffer(const Device &device, uint64_t size);
	virtual ~Buffer();

	void upload_data(const void *data);

	void map();
	void unmap();
	void *get_mapped_memory();
	const vk::raii::Buffer &get_buffer() const;

	Buffer(const Buffer &) = delete;
	Buffer &operator=(const Buffer &) = delete;
	Buffer(Buffer &&);
	Buffer &operator=(Buffer &&);

protected:
	struct Impl;
	core::UniquePtr<Impl> impl;
};

class StagingBuffer : public Buffer {
public:
	StagingBuffer(const Device &device, uint64_t size);
	~StagingBuffer() = default;

	StagingBuffer(const StagingBuffer &) = delete;
	StagingBuffer &operator=(const StagingBuffer &) = delete;
	StagingBuffer(StagingBuffer &&) = default;
	StagingBuffer &operator=(StagingBuffer &&) = default;
};

class VertexBuffer : public Buffer {
public:
	VertexBuffer(const Device &device, uint64_t size);
	~VertexBuffer() = default;

	VertexBuffer(const VertexBuffer &) = delete;
	VertexBuffer &operator=(const VertexBuffer &) = delete;
	VertexBuffer(VertexBuffer &&) = default;
	VertexBuffer &operator=(VertexBuffer &&) = default;
};

class IndexBuffer : public Buffer {
public:
	IndexBuffer(const Device &device, uint64_t size);
	~IndexBuffer() = default;

	IndexBuffer(const IndexBuffer &) = delete;
	IndexBuffer &operator=(const IndexBuffer &) = delete;
	IndexBuffer(IndexBuffer &&) = default;
	IndexBuffer &operator=(IndexBuffer &&) = default;
};

class UniformBuffer : public Buffer {
public:
	UniformBuffer(const Device &device, uint64_t size);
	~UniformBuffer() = default;

	UniformBuffer(const UniformBuffer &) = delete;
	UniformBuffer &operator=(const UniformBuffer &) = delete;
	UniformBuffer(UniformBuffer &&) = default;
	UniformBuffer &operator=(UniformBuffer &&) = default;
};

} // namespace tramogi::graphics
