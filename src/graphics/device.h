#pragma once

#include "tramogi/core/errors.h"
#include <memory>
#include <stdint.h>

namespace vk {
class Fence;
class PresentInfoKHR;
class SubmitInfo;
namespace raii {
class Device;
class Semaphore;
} // namespace raii
} // namespace vk

namespace tramogi::graphics {

class Instance;
class PhysicalDevice;

class Device {
public:
	Device(const PhysicalDevice &physical_device);
	~Device();
	Device(const Device &) = delete;
	Device &operator=(const Device &) = delete;
	Device(Device &&);
	Device &operator=(Device &&) = delete;

	void init(const Instance &instance);

	void submit_graphics(
		vk::SubmitInfo submit_info,
		uint32_t frame_index = 0,
		bool wait_for_fence = false
	);
	core::Result<> present(vk::PresentInfoKHR present_info);

	void wait_idle(uint32_t frame_index) const;
	void wait_graphics_queue() const;
	void reset_fence(uint32_t frame_index);

	const PhysicalDevice &get_physical_device() const {
		return physical_device;
	}

	const vk::raii::Device &get_device() const;
	const vk::raii::Semaphore &get_render_semaphore(uint32_t frame_index) const;
	const vk::raii::Semaphore &get_present_semaphore(uint32_t frame_index) const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;

	const PhysicalDevice &physical_device;

	void create_sync_objects();
};

} // namespace tramogi::graphics
