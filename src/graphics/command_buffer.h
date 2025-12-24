#pragma once

#include <memory>

namespace vk::raii {
class CommandBuffer;
}

namespace tramogi::graphics {

class CommandBuffer {
public:
	CommandBuffer() = delete;
	CommandBuffer(vk::raii::CommandBuffer &&command_buffer);
	~CommandBuffer();

	CommandBuffer(const CommandBuffer &) = delete;
	CommandBuffer &operator=(const CommandBuffer &) = delete;
	CommandBuffer(CommandBuffer &&);
	CommandBuffer &operator=(CommandBuffer &&);

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace tramogi::graphics
