#include "instance.h"
#include "tramogi/core/errors.h"
#include "tramogi/core/logging/logging.h"
#include "vulkan/vulkan.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace tramogi::graphics {

using core::Error;
using core::Result;
using core::logging::debug_log;

constexpr std::array<const char *, 1> validation_layers = {
	"VK_LAYER_KHRONOS_validation",
};
#ifdef NDEBUG
constexpr bool enable_validation_layer = false;
#else
constexpr bool enable_validation_layer = true;
#endif

struct Instance::Impl {
	vk::raii::Context context {};
	vk::raii::Instance instance = nullptr;
	vk::raii::DebugUtilsMessengerEXT debug_messenger = nullptr;
	vk::PhysicalDevice physical_device = nullptr;
};

Instance::Instance() : impl(std::make_unique<Impl>()) {}
Instance::~Instance() = default;
Instance::Instance(Instance &&) = default;
Instance &Instance::operator=(Instance &&) = default;

bool check_extensions(
	const vk::raii::Context &context,
	const std::vector<const char *> &required_extensions
) {
	bool is_requirements_met = true;
	std::vector<vk::ExtensionProperties> available_extensions =
		context.enumerateInstanceExtensionProperties();
	debug_log("Required Extensions:", required_extensions.size());
	for (auto required_ext : required_extensions) {
		bool is_available = !std::ranges::none_of(
			available_extensions,
			[&required_ext](const vk::ExtensionProperties &available_ext) {
				return std::strcmp(required_ext, available_ext.extensionName) == 0;
			}
		);
		is_requirements_met = is_requirements_met && is_available;
		debug_log("  - {}: {}", required_ext, is_available ? "OK" : "NO");
	}
	return is_requirements_met;
}

bool check_layers(const vk::raii::Context &context, std::vector<const char *> required_layers) {
	bool is_requirements_met = true;
	std::vector<vk::LayerProperties> available_layers = context.enumerateInstanceLayerProperties();
	debug_log("Required Layers:");
	for (auto required_layer : required_layers) {
		bool is_available = !std::ranges::none_of(
			available_layers,
			[&required_layer](const vk::LayerProperties &available_layer) {
				return strcmp(required_layer, available_layer.layerName) == 0;
			}
		);
		is_requirements_met = is_requirements_met && is_available;
		debug_log("  - {}: {}", required_layer, is_available ? "OK" : "NO");
	}
	return is_requirements_met;
}

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
	vk::DebugUtilsMessageTypeFlagsEXT type,
	const vk::DebugUtilsMessengerCallbackDataEXT *data,
	void *
) {
	debug_log(
		"Validation layer: [{}] {}: {}",
		vk::to_string(severity),
		vk::to_string(type),
		data->pMessage
	);
	return vk::False;
}

vk::raii::DebugUtilsMessengerEXT setup_debug_messenger(const vk::raii::Instance &instance) {
	vk::DebugUtilsMessageSeverityFlagsEXT severity_flags {
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
	};
	vk::DebugUtilsMessageTypeFlagsEXT type_flags {
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
	};

	vk::DebugUtilsMessengerCreateInfoEXT create_info {
		.messageSeverity = severity_flags,
		.messageType = type_flags,
		.pfnUserCallback = &debug_callback,
	};

	return instance.createDebugUtilsMessengerEXT(create_info);
}

Result<> Instance::init(const std::vector<const char *> &base_required_extensions) {
	vk::ApplicationInfo app_info {
		.pApplicationName = "Tramogi",
		.applicationVersion = vk::makeVersion(0, 1, 0),
		.pEngineName = "Tramogi Engine",
		.engineVersion = vk::makeVersion(0, 1, 0),
		.apiVersion = vk::ApiVersion14,
	};

	std::vector<const char *> required_extensions {base_required_extensions};
	std::vector<const char *> required_layers;
	if constexpr (enable_validation_layer) {
		required_layers.assign(validation_layers.cbegin(), validation_layers.cend());
		required_extensions.push_back(vk::EXTDebugUtilsExtensionName);
	}
	if (!check_layers(impl->context, required_layers)) {
		return Error("Required layers not available");
	}

	if (!check_extensions(impl->context, required_extensions)) {
		return Error("Required extensions not available");
	}

	vk::InstanceCreateInfo create_info {
		.pApplicationInfo = &app_info,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size()),
		.ppEnabledExtensionNames = required_extensions.data(),
	};

	impl->instance = vk::raii::Instance(impl->context, create_info);

	if constexpr (enable_validation_layer) {
		impl->debug_messenger = setup_debug_messenger(impl->instance);
	}

	return {};
}

std::vector<vk::raii::PhysicalDevice> Instance::get_physical_devices() const {
	return impl->instance.enumeratePhysicalDevices();
}

const vk::raii::Instance &Instance::get_instance() const {
	return impl->instance;
}

} // namespace tramogi::graphics
