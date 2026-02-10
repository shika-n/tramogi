#include "physical_device.h"
#include "format.h"
#include "instance.h"
#include "surface.h"
#include "tramogi/core/logging/logging.h"
#include "tramogi/core/types.h"
#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace tramogi::graphics {

using core::Error;
using core::Optional;
using core::Result;
using core::logging::debug_log;

struct PhysicalDevice::Impl {
	vk::raii::PhysicalDevice physical_device = nullptr;
};

const std::vector<const char *> PhysicalDevice::required_device_extensions {
	vk::KHRSwapchainExtensionName,
	vk::KHRSpirv14ExtensionName,
	vk::KHRSynchronization2ExtensionName,
	vk::KHRCreateRenderpass2ExtensionName,
	vk::KHRMaintenance6ExtensionName,
};

DeviceSuitableness get_device_suitableness(
	vk::raii::PhysicalDevice physical_device,
	vk::SurfaceKHR surface
) {
	bool is_suitable = true;
	[[maybe_unused]] bool is_api_supported = true;
	[[maybe_unused]] bool anisotropy_support = true;
	std::map<const char *, bool> extension_support_map;

	auto property = physical_device.getProperties();

	if (property.apiVersion < VK_API_VERSION_1_3) {
		is_suitable = false;
		is_api_supported = false;
	}

	auto available_extensions = physical_device.enumerateDeviceExtensionProperties();
	for (const auto &extension : PhysicalDevice::required_device_extensions) {
		extension_support_map[extension] = true;
		auto iter = std::ranges::find_if(
			available_extensions,
			[extension](const auto &available_extension) {
				return std::strcmp(available_extension.extensionName, extension);
			}
		);
		if (iter == available_extensions.end()) {
			is_suitable = false;
			extension_support_map[extension] = false;
		}
	}

	auto supported_features = physical_device.getFeatures();
	if (!supported_features.samplerAnisotropy) {
		is_suitable = false;
		anisotropy_support = false;
	}

	// TODO: Improve queue query
	auto queue_families = physical_device.getQueueFamilyProperties();
	uint32_t graphics_queue_index = queue_families.size();
	uint32_t present_queue_index = queue_families.size();
	for (uint32_t i = 0; i < queue_families.size(); ++i) {
		const auto &queue_family = queue_families.at(i);

		if (graphics_queue_index == queue_families.size() &&
			(queue_family.queueFlags & vk::QueueFlagBits::eGraphics) !=
				static_cast<vk::QueueFlags>(0)) {
			graphics_queue_index = i;
		}

		if (present_queue_index == queue_families.size() &&
			physical_device.getSurfaceSupportKHR(i, surface)) {
			present_queue_index = i;
		}
	}

	if (graphics_queue_index == queue_families.size() ||
		present_queue_index == queue_families.size()) {
		is_suitable = false;
	}

	debug_log("Physical Device: {}", std::string(property.deviceName));
	debug_log("  Vulkan API v1.3 Support: {}", is_api_supported);
	debug_log("  Extensions:");
	for ([[maybe_unused]] auto entry : extension_support_map) {
		debug_log("    - {}: {}", entry.first, entry.second ? "Yes" : "No");
	}
	debug_log("  Anisotropy Support: {}", anisotropy_support);
	debug_log("  Queue:");
	debug_log(
		"    Graphics Queue Index: {}",
		graphics_queue_index == queue_families.size() ? "Not Found"
													  : std::to_string(graphics_queue_index)
	);
	debug_log(
		"    Present Queue Index: {}",
		present_queue_index == queue_families.size() ? "Not Found"
													 : std::to_string(present_queue_index)
	);

	return {
		.is_suitable = is_suitable,
		.graphics_queue_index = graphics_queue_index,
		.present_queue_index = present_queue_index
	};
}

uint32_t get_device_score(vk::raii::PhysicalDevice device) {
	uint32_t score = 0;

	auto property = device.getProperties();
	if (property.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
		score += 1000;
	} else if (property.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
		score += 200;
	} else {
		score += 100;
	}

	return score;
}

PhysicalDevice::PhysicalDevice(const Instance &instance, const vk::SurfaceKHR &surface_khr)
	: impl(std::make_unique<Impl>()), surface(instance, surface_khr) {
	auto physical_devices = instance.get_physical_devices();
	if (physical_devices.empty()) {
		throw std::runtime_error("No GPU that supports Vulkan found");
	}

	std::unordered_map<vk::raii::PhysicalDevice *, DeviceSuitableness> device_suitableness_map;
	std::ranges::for_each(physical_devices, [this, &device_suitableness_map](auto &device) {
		device_suitableness_map[&device] = get_device_suitableness(device, surface.get_surface());
	});

	auto suitable_devices =
		std::ranges::filter_view(physical_devices, [&device_suitableness_map](auto &device) {
			return device_suitableness_map[&device].is_suitable;
		});

	std::multimap<uint32_t, vk::raii::PhysicalDevice *, std::greater<uint32_t>> device_scores;
	for (auto &device : suitable_devices) {
		uint32_t score = get_device_score(device);
		device_scores.insert(std::make_pair(score, &device));
	}

	for (auto score_pair : device_scores) {
		if (score_pair.first > 0) {
			impl->physical_device = *score_pair.second;
			device_suitableness = device_suitableness_map[score_pair.second];
			break;
		}
	}

	if (impl->physical_device == nullptr) {
		throw std::runtime_error("No suitable device found");
	}

	debug_log("Using: {}", impl->physical_device.getProperties().deviceName.data());
}

PhysicalDevice::~PhysicalDevice() = default;
PhysicalDevice::PhysicalDevice(PhysicalDevice &&) = default;
PhysicalDevice &PhysicalDevice::operator=(PhysicalDevice &&) = default;

Result<Format> PhysicalDevice::get_depth_format() const {
	std::array formats {
		Format::Depth32Stencil,
		Format::Depth24Stencil,
	};

	for (const auto &format : formats) {
		auto format_properties = impl->physical_device.getFormatProperties(native(format));
		if ((format_properties.optimalTilingFeatures &
			 vk::FormatFeatureFlagBits::eDepthStencilAttachment) ==
			vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
			return format;
		}
	}

	return Error("Failed to find a suitable depth format");
}

const vk::raii::PhysicalDevice &PhysicalDevice::get_physical_device() const {
	return impl->physical_device;
}

vk::PhysicalDeviceMemoryProperties PhysicalDevice::get_memory_properties() const {
	return impl->physical_device.getMemoryProperties();
}

vk::SurfaceCapabilitiesKHR PhysicalDevice::get_surface_capabilities() const {
	return impl->physical_device.getSurfaceCapabilitiesKHR(surface.get_surface());
}

Optional<Format> internal_format(vk::Format format) {
	switch (format) {
	case vk::Format::eB8G8R8A8Srgb:
		return Format::BGRA8Srgb;
	case vk::Format::eB8G8R8A8Unorm:
		return Format::BGRA8Unorm;
	default:
		return core::optional::none;
	}
}

std::vector<SurfaceFormat> PhysicalDevice::get_surface_formats() const {
	std::vector<SurfaceFormat> formats;
	for (const auto &surface_format :
		 impl->physical_device.getSurfaceFormatsKHR(surface.get_surface())) {
		Optional<Format> format = internal_format(surface_format.format);
		if (!format) {
			continue;
		}
		formats.emplace_back(
			SurfaceFormat {
				.format = format.value(),
				.is_nonlinear = surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear,
			}
		);
	}
	return formats;
}

std::vector<vk::PresentModeKHR> PhysicalDevice::get_surface_present_modes() const {
	return impl->physical_device.getSurfacePresentModesKHR(surface.get_surface());
}

} // namespace tramogi::graphics
