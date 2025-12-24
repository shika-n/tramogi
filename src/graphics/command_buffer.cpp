#include "command_buffer.h"
#include "../logging.h"
#include "vulkan/vulkan.hpp"
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

struct CommandBuffer::Impl {
	vk::raii::CommandBuffer command_buffer = nullptr;
};

CommandBuffer::CommandBuffer(vk::raii::CommandBuffer &&command_buffer)
	: impl(std::make_unique<Impl>()), type(CommandBufferType::Multiple) {
	impl->command_buffer = std::move(command_buffer);
}
CommandBuffer::~CommandBuffer() {
	DLOG("cmd DTOR ");
};
CommandBuffer::CommandBuffer(CommandBuffer &&) = default;
CommandBuffer &CommandBuffer::operator=(CommandBuffer &&) = default;

void CommandBuffer::begin() const {
	vk::CommandBufferBeginInfo begin_info {};
	impl->command_buffer.begin(begin_info);
}

void CommandBuffer::begin_onetime() {
	type = CommandBufferType::OneTime;
	vk::CommandBufferBeginInfo begin_info {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
	};
	impl->command_buffer.begin(begin_info);
}

void CommandBuffer::end() const {
	impl->command_buffer.end();
}

const vk::raii::CommandBuffer &CommandBuffer::get_command_buffer() const {
	return impl->command_buffer;
}

} // namespace tramogi::graphics
