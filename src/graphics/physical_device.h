#pragma once

#include "surface.h"
#include "tramogi/core/errors.h"
#include <memory>
#include <stdint.h>
#include <vector>

namespace vk {
enum class Format;
enum class PresentModeKHR;
class PhysicalDeviceMemoryProperties;
class SurfaceCapabilitiesKHR;
class SurfaceFormatKHR;
class SurfaceKHR;
namespace raii {
class PhysicalDevice;
}
} // namespace vk

namespace tramogi::graphics {

class Instance;

struct DeviceSuitableness {
	bool is_suitable = false;
	uint32_t graphics_queue_index = 0;
	uint32_t present_queue_index = 0;
};

class PhysicalDevice {
public:
	static const std::vector<const char *> required_device_extensions;

	PhysicalDevice();
	~PhysicalDevice();
	PhysicalDevice(const PhysicalDevice &) = delete;
	PhysicalDevice &operator=(const PhysicalDevice &) = delete;
	PhysicalDevice(PhysicalDevice &&);
	PhysicalDevice &operator=(PhysicalDevice &&);

	core::Result<> init(const Instance &instance, const vk::SurfaceKHR &surface_khr);

	uint32_t get_graphics_queue_index() const {
		return device_suitableness.graphics_queue_index;
	}
	uint32_t get_present_queue_index() const {
		return device_suitableness.present_queue_index;
	}

	core::Result<vk::Format> get_depth_format();

	const vk::raii::PhysicalDevice &get_physical_device() const;
	const vk::raii::SurfaceKHR &get_surface() const {
		return surface.get_surface();
	}

	vk::PhysicalDeviceMemoryProperties get_memory_properties() const;
	vk::SurfaceCapabilitiesKHR get_surface_capabilities() const;
	std::vector<vk::SurfaceFormatKHR> get_surface_formats() const;
	std::vector<vk::PresentModeKHR> get_surface_present_modes() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
	Surface surface;
	DeviceSuitableness device_suitableness;
};

} // namespace tramogi::graphics
