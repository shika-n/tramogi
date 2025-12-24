#include "command_buffer.h"
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

struct CommandBuffer::Impl {
	vk::raii::CommandBuffer command_buffer = nullptr;
};

CommandBuffer::CommandBuffer(vk::raii::CommandBuffer &&command_buffer)
	: impl(std::make_unique<Impl>()) {
	impl->command_buffer = std::move(command_buffer);
}
CommandBuffer::~CommandBuffer() = default;
CommandBuffer::CommandBuffer(CommandBuffer &&) = default;
CommandBuffer &CommandBuffer::operator=(CommandBuffer &&) = default;

} // namespace tramogi::graphics
