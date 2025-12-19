#include "device.h"
#include "dispatch_loader.h"
#include "instance.h"
#include "physical_device.h"
#include "tramogi/core/errors.h"
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
};

Device::Device(const PhysicalDevice &physical_device)
	: impl(std::make_unique<Impl>()), physical_device(physical_device) {}
Device::~Device() = default;
Device::Device(Device &&) = default;
// Device &Device::operator=(Device &&) = default;

void Device::init(const Instance &instance) {
	float priority = 0.0f;
	vk::DeviceQueueCreateInfo device_queue_create_info {
		.queueFamilyIndex = physical_device.get_graphics_queue_index(),
		.queueCount = 1,
		.pQueuePriorities = &priority,
	};
	vk::StructureChain<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan11Features,
		vk::PhysicalDeviceVulkan13Features,
		vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		feature_chain {
			{.features = {.samplerAnisotropy = vk::True}},
			{.shaderDrawParameters = true},
			{.synchronization2 = true, .dynamicRendering = true},
			{.extendedDynamicState = true},
		};

	vk::DeviceCreateInfo device_create_info {
		.pNext = &feature_chain.get<vk::PhysicalDeviceFeatures2>(),
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &device_queue_create_info,
		.enabledExtensionCount =
			static_cast<uint32_t>(PhysicalDevice::required_device_extensions.size()),
		.ppEnabledExtensionNames = PhysicalDevice::required_device_extensions.data(),
	};

	impl->device = vk::raii::Device(physical_device.get_physical_device(), device_create_info);
	impl->graphics_queue =
		vk::raii::Queue(impl->device, physical_device.get_graphics_queue_index(), 0);
	impl->present_queue =
		vk::raii::Queue(impl->device, physical_device.get_present_queue_index(), 0);

	create_sync_objects();

	init_loader(instance.get_instance(), impl->device);
}

void Device::submit_graphics(
	vk::SubmitInfo submit_info,
	uint32_t frame_index,
	bool wait_for_fence
) {
	if (wait_for_fence) {
		impl->graphics_queue.submit(submit_info, impl->fences[frame_index]);
	} else {
		impl->graphics_queue.submit(submit_info, nullptr);
	}
}

Result<> Device::present(vk::PresentInfoKHR present_info) {
	try {
		vk::Result result = impl->present_queue.presentKHR(present_info);
		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
			return Error("Present out of dat");
		}
	} catch (const vk::SystemError &e) {
		if (e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR)) {
			return Error("Present out of dat");
		} else {
			return Error("Unexpected present error");
		}
	}

	return {};
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
