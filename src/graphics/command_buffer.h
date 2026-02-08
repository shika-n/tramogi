#pragma once

#include "tramogi/core/pointers.h"

namespace vk {
class CommandBuffer;
namespace raii {
class CommandBuffer;
}
} // namespace vk

namespace tramogi::graphics {

enum class CommandBufferType {
	OneTime,
	Multiple,
};

class CommandBuffer {
public:
	CommandBuffer() = delete;
	CommandBuffer(vk::raii::CommandBuffer &&command_buffer);
	~CommandBuffer();

	CommandBuffer(const CommandBuffer &) = delete;
	CommandBuffer &operator=(const CommandBuffer &) = delete;
	CommandBuffer(CommandBuffer &&);
	CommandBuffer &operator=(CommandBuffer &&);

	void begin() const;
	void begin_onetime();
	void end() const;

	CommandBufferType get_type() const {
		return type;
	}

	const vk::raii::CommandBuffer &get_command_buffer() const;

private:
	struct Impl;
	core::UniquePtr<Impl> impl;

	CommandBufferType type;
};

} // namespace tramogi::graphics
