#pragma once

#include "tramogi/core/types.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace vk {
class Fence;
class PresentInfoKHR;
class SubmitInfo;
namespace raii {
class CommandBuffer;
class CommandPool;
class Device;
class Semaphore;
class Queue;
} // namespace raii
} // namespace vk

namespace tramogi::graphics {

class CommandBuffer;
class DescriptorLayout;
class DescriptorPool;
class DescriptorSet;
class Instance;
class PhysicalDevice;
class Swapchain;

class Device {
public:
	Device(const PhysicalDevice &physical_device, const Instance &instance);
	~Device();
	Device(const Device &) = delete;
	Device &operator=(const Device &) = delete;
	Device(Device &&);
	Device &operator=(Device &&) = delete;

	void submit_graphics_single(const CommandBuffer &command_buffer);
	void submit_graphics(const CommandBuffer &command_buffer, uint32_t frame_index);
	core::Result<> present(const Swapchain &swapchain, uint32_t image_index, uint32_t frame_index);

	CommandBuffer allocate_command_buffer() const;
	std::vector<CommandBuffer> allocate_command_buffers(uint32_t count) const;
	std::vector<DescriptorSet> allocate_descriptor_sets(
		const DescriptorPool &descriptor_pool,
		const DescriptorLayout &descriptor_set_layout,
		uint32_t count
	) const;

	void wait_idle(uint32_t frame_index) const;
	void wait_graphics_queue() const;
	void reset_fence(uint32_t frame_index);

	const PhysicalDevice &get_physical_device() const {
		return physical_device;
	}

	const vk::raii::Device &get_device() const;
	const vk::raii::CommandPool &get_command_pool() const;
	const vk::raii::Queue &get_graphics_queue() const;
	const vk::raii::Semaphore &get_render_semaphore(uint32_t frame_index) const;
	const vk::raii::Semaphore &get_present_semaphore(uint32_t frame_index) const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;

	const PhysicalDevice &physical_device;

	void create_sync_objects();
};

} // namespace tramogi::graphics
