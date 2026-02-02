#include "device.h"
#include "command_buffer.h"
#include "descriptor.h"
#include "descriptor_pool.h"
#include "dispatch_loader.h"
#include "instance.h"
#include "physical_device.h"
#include "swapchain.h"
#include "tramogi/core/types.h"
#include <cstdint>
#include <limits>
#include <memory>
#include <stdint.h>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

using core::Error;
using core::Result;

struct Device::Impl {
	vk::raii::Device device = nullptr;
	vk::raii::Queue graphics_queue = nullptr;
	vk::raii::Queue present_queue = nullptr;

	std::vector<vk::raii::Semaphore> render_semaphores;
	std::vector<vk::raii::Semaphore> present_semaphores;
	std::vector<vk::raii::Fence> fences;

	vk::raii::CommandPool command_pool = nullptr;

	void init_command_pool(const PhysicalDevice &physical_device) {
		vk::CommandPoolCreateInfo create_info {
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = physical_device.get_graphics_queue_index(),
		};
		command_pool = vk::raii::CommandPool(device, create_info);
	}
};

Device::Device(const PhysicalDevice &physical_device, const Instance &instance)
	: impl(std::make_unique<Impl>()), physical_device(physical_device) {
	float priority = 0.0f;
	vk::DeviceQueueCreateInfo device_queue_create_info {
		.queueFamilyIndex = physical_device.get_graphics_queue_index(),
		.queueCount = 1,
		.pQueuePriorities = &priority,
	};
	vk::StructureChain<
		vk::DeviceCreateInfo,
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan11Features,
		vk::PhysicalDeviceVulkan13Features,
		vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		device_info {
			{
				.queueCreateInfoCount = 1,
				.pQueueCreateInfos = &device_queue_create_info,
				.enabledExtensionCount =
					static_cast<uint32_t>(PhysicalDevice::required_device_extensions.size()),
				.ppEnabledExtensionNames = PhysicalDevice::required_device_extensions.data(),
			},
			{.features = {.samplerAnisotropy = vk::True}},
			{.shaderDrawParameters = true},
			{.synchronization2 = true, .dynamicRendering = true},
			{.extendedDynamicState = true},
		};

	impl->device = vk::raii::Device(physical_device.get_physical_device(), device_info.get());
	init_loader(instance.get_instance(), impl->device);

	impl->graphics_queue =
		vk::raii::Queue(impl->device, physical_device.get_graphics_queue_index(), 0);
	impl->present_queue =
		vk::raii::Queue(impl->device, physical_device.get_present_queue_index(), 0);

	create_sync_objects();

	impl->init_command_pool(physical_device);
}

Device::~Device() = default;
Device::Device(Device &&) = default;
// Device &Device::operator=(Device &&) = default;

void Device::submit_graphics_onetime(const CommandBuffer &command_buffer) {
	assert(
		command_buffer.get_type() == CommandBufferType::OneTime &&
		"Only CommandBuffer of type OneTime maybe submitted with submit_graphics_single()"
	);

	vk::SubmitInfo submit_info {
		.commandBufferCount = 1,
		.pCommandBuffers = &*command_buffer.get_command_buffer(),
	};
	impl->graphics_queue.submit(submit_info);
	wait_graphics_queue();
}

void Device::submit_graphics(const CommandBuffer &command_buffer, uint32_t frame_index) {
	vk::PipelineStageFlags wait_destination_stage_mask(
		vk::PipelineStageFlagBits::eColorAttachmentOutput
	);

	const vk::SubmitInfo submit_info {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*impl->present_semaphores[frame_index],
		.pWaitDstStageMask = &wait_destination_stage_mask,
		.commandBufferCount = 1,
		.pCommandBuffers = &*command_buffer.get_command_buffer(),
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &*impl->render_semaphores[frame_index],
	};

	impl->graphics_queue.submit(submit_info, impl->fences[frame_index]);
}

Result<> Device::present(const Swapchain &swapchain, uint32_t image_index, uint32_t frame_index) {
	vk::PresentInfoKHR present_info {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*impl->render_semaphores[frame_index],
		.swapchainCount = 1,
		.pSwapchains = &*swapchain.get_swapchain(),
		.pImageIndices = &image_index,
	};

	try {
		vk::Result result = impl->present_queue.presentKHR(present_info);
		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
			return Error("Present out of date");
		}
	} catch (const vk::SystemError &e) {
		if (e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR) ||
			e.code().value() == static_cast<int>(vk::Result::eSuboptimalKHR)) {
			return Error("Present out of date");
		} else {
			throw std::runtime_error("Unexpected present result");
		}
	}

	return {};
}

CommandBuffer Device::allocate_command_buffer() const {
	return std::move(allocate_command_buffers(1).front());
}

std::vector<CommandBuffer> Device::allocate_command_buffers(uint32_t count) const {
	assert(count > 0);
	vk::CommandBufferAllocateInfo allocate_info {
		.commandPool = impl->command_pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = count,
	};

	std::vector<CommandBuffer> command_buffers;
	for (auto &command_buffer : impl->device.allocateCommandBuffers(allocate_info)) {
		command_buffers.emplace_back(std::move(command_buffer));
	}

	return command_buffers;
}

std::vector<DescriptorSet> Device::allocate_descriptor_sets(
	const DescriptorPool &descriptor_pool,
	const DescriptorLayout &descriptor_set_layout,
	uint32_t count
) const {
	std::vector<vk::DescriptorSetLayout> layouts(count, descriptor_set_layout.get_layout());
	vk::DescriptorSetAllocateInfo allocate_info {
		.descriptorPool = descriptor_pool.get_descriptor_pool(),
		.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
		.pSetLayouts = layouts.data(),
	};

	std::vector<DescriptorSet> descriptor_sets;
	for (auto &descriptor_set : impl->device.allocateDescriptorSets(allocate_info)) {
		descriptor_sets.emplace_back(std::move(descriptor_set));
	}
	return descriptor_sets;
}

void Device::wait_idle(uint32_t frame_index) const {
	impl->device.waitIdle();
	while (vk::Result::eTimeout == impl->device.waitForFences(
									   *impl->fences[frame_index],
									   vk::True,
									   std::numeric_limits<uint64_t>().max()
								   ))
		;
}

void Device::wait_graphics_queue() const {
	impl->graphics_queue.waitIdle();
}

void Device::reset_fence(uint32_t frame_index) {
	impl->device.resetFences(*impl->fences[frame_index]);
}

const vk::raii::Device &Device::get_device() const {
	return impl->device;
}

const vk::raii::CommandPool &Device::get_command_pool() const {
	return impl->command_pool;
}

const vk::raii::Queue &Device::get_graphics_queue() const {
	return impl->graphics_queue;
}

const vk::raii::Semaphore &Device::get_render_semaphore(uint32_t frame_index) const {
	return impl->render_semaphores[frame_index];
}

const vk::raii::Semaphore &Device::get_present_semaphore(uint32_t frame_index) const {
	return impl->present_semaphores[frame_index];
}

void Device::create_sync_objects() {
	impl->present_semaphores.clear();
	impl->render_semaphores.clear();
	impl->fences.clear();

	// TODO: Change 2 to MAX_FRAME_IN_FLIGHT
	for (uint32_t i = 0; i < 2; ++i) {
		impl->present_semaphores.emplace_back(impl->device, vk::SemaphoreCreateInfo());
		impl->render_semaphores.emplace_back(impl->device, vk::SemaphoreCreateInfo());
		impl->fences.emplace_back(
			impl->device,
			vk::FenceCreateInfo {.flags = vk::FenceCreateFlagBits::eSignaled}
		);
	}
}

} // namespace tramogi::graphics
