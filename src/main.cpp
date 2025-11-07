#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iterator>
#include <map>
#include <print>
#include <stdexcept>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "macros.h"

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;

// TODO: Add validation layers

class ProjectSkyHigh {
public:
	void run() {
		init_window();
		init_vulkan();
		main_loop();
		cleanup();
	}

private:
	GLFWwindow* window = nullptr;

	vk::raii::Context context;
	vk::raii::Instance instance = nullptr;
	vk::raii::PhysicalDevice physical_device = nullptr;
	vk::raii::Device device = nullptr;
	vk::raii::Queue graphics_queue = nullptr;

	std::vector<char const*> required_device_extensions = {
		vk::KHRSwapchainExtensionName,
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName,
		vk::KHRCreateRenderpass2ExtensionName,
	};

	void init_window() {
		if (!glfwInit()) {
			throw std::runtime_error("Failed to initialize GLFW");
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(
			WIDTH, HEIGHT, "Project Sky-High", nullptr, nullptr
		);
	}

	void init_vulkan() {
		create_instance();
		pick_physical_device();
		create_logical_device();
	}

	void main_loop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
		}
	}

	void cleanup() {
		glfwDestroyWindow(window);

		glfwTerminate();
	}

	void create_instance() {
		constexpr vk::ApplicationInfo app_info {
			.pApplicationName = "Project Sky-High",
			.applicationVersion = VK_MAKE_VERSION(0, 1, 0),
			.pEngineName = "Sky-High Engine",
			.engineVersion = VK_MAKE_VERSION(0, 1, 0),
			.apiVersion = vk::ApiVersion14,
		};

		uint32_t extension_count = 0;
		auto extensions = glfwGetRequiredInstanceExtensions(&extension_count);

		auto available_extensions =
			context.enumerateInstanceExtensionProperties();

		for (uint32_t i = 0; i < extension_count; ++i) {
			if (std::ranges::none_of(
					available_extensions,
					[extension =
						 extensions[i]](auto const& available_extension) {
						return std::strcmp(
								   available_extension.extensionName, extension
							   ) == 0;
					}
				)) {
				throw std::runtime_error(
					"Required extension not supported: " +
					std::string(extensions[i])
				);
			}
		}

		vk::InstanceCreateInfo create_info {
			.pApplicationInfo = &app_info,
			.enabledExtensionCount = extension_count,
			.ppEnabledExtensionNames = extensions,
		};

		instance = vk::raii::Instance(context, create_info);
	}

	uint32_t get_device_score(vk::raii::PhysicalDevice const& device) {
		uint32_t score = 0;
		bool api_version_support = false;
		bool is_discreet = false;
		bool required_queue_available = true;
		bool required_extensions_available = true;

		auto property = device.getProperties();
		if (property.apiVersion >= VK_API_VERSION_1_3) {
			api_version_support = true;
		}
		if (property.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
			score += 1000;
		} else if (property.deviceType ==
				   vk::PhysicalDeviceType::eIntegratedGpu) {
			score += 100;
		}

		auto queue_families = device.getQueueFamilyProperties();
		auto const queue_family_iter = std::ranges::find_if(
			queue_families, [](vk::QueueFamilyProperties const& queue_family) {
				return (queue_family.queueFlags &
						vk::QueueFlagBits::eGraphics) !=
					   static_cast<vk::QueueFlags>(0);
			}
		);
		if (queue_family_iter == queue_families.end()) {
			required_queue_available = false;
		}

		auto available_extensions = device.enumerateDeviceExtensionProperties();
		for (auto const& extension : required_device_extensions) {
			auto iter = std::ranges::find_if(
				available_extensions,
				[extension](auto const& available_extension) {
					return std::strcmp(
						available_extension.extensionName, extension
					);
				}
			);
			required_extensions_available = required_extensions_available &&
											iter != available_extensions.end();
		}

		if (!api_version_support || !required_queue_available ||
			!required_extensions_available) {
			score = 0;
		}

		DLOG("Device Detected: {}", device.getProperties().deviceName.data());
		DLOG("> Vulkan API 1.3 support: {}", api_version_support);
		DLOG("> Discreet GPU: {}", is_discreet);
		DLOG("> Required Queues Available: {}", required_queue_available);
		DLOG(
			"> Required Extensions Available: {}", required_extensions_available
		);
		DLOG("> Score: {}", score);

		return score;
	}

	uint32_t get_queue_family(vk::raii::PhysicalDevice device) {
		auto queue_families = device.getQueueFamilyProperties();
		auto const queue_family_iter = std::ranges::find_if(
			queue_families, [](vk::QueueFamilyProperties const& queue_family) {
				return (queue_family.queueFlags &
						vk::QueueFlagBits::eGraphics) !=
					   static_cast<vk::QueueFlags>(0);
			}
		);
		return std::distance(queue_families.begin(), queue_family_iter);
	}

	void pick_physical_device() {
		auto devices = instance.enumeratePhysicalDevices();
		if (devices.empty()) {
			throw std::runtime_error("No GPU that supports Vulkan found");
		}

		std::multimap<uint32_t, vk::raii::PhysicalDevice> device_scores;
		for (auto const& device : devices) {
			uint32_t score = get_device_score(device);
			device_scores.insert(std::make_pair(score, device));
		}

		for (auto score_pair : device_scores) {
			if (score_pair.first > 0) {
				physical_device = score_pair.second;
				break;
			}
		}

		if (physical_device == nullptr) {
			throw std::runtime_error("No suitable device found");
		}

		DLOG("Using: {}", physical_device.getProperties().deviceName.data());
	}

	void create_logical_device() {
		uint32_t queue_family_index = get_queue_family(physical_device);
		float priority = 0.0f;
		vk::DeviceQueueCreateInfo device_queue_create_info {
			.queueFamilyIndex = queue_family_index,
			.queueCount = 1,
			.pQueuePriorities = &priority,
		};
		vk::StructureChain<
			vk::PhysicalDeviceFeatures2,
			vk::PhysicalDeviceVulkan13Features,
			vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
			feature_chain {
				{},
				{.dynamicRendering = true},
				{.extendedDynamicState = true},
			};

		vk::DeviceCreateInfo device_create_info {
			.pNext = &feature_chain.get<vk::PhysicalDeviceFeatures2>(),
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &device_queue_create_info,
			.enabledExtensionCount =
				static_cast<uint32_t>(required_device_extensions.size()),
			.ppEnabledExtensionNames = required_device_extensions.data(),
		};

		device = vk::raii::Device(physical_device, device_create_info);
		graphics_queue = vk::raii::Queue(device, queue_family_index, 0);
	}
};

int main() {
	DLOG("Running in DEBUG mode");

	ProjectSkyHigh skyhigh;

	try {
		skyhigh.run();
	} catch (vk::SystemError const& e) {
		std::println(stderr, "Vulkan Error: {}", e.what());
		return EXIT_FAILURE;
	} catch (std::exception const& e) {
		std::println(stderr, "Error: {}", e.what());
		return EXIT_FAILURE;
	}

	DLOG("Exited successfully");
	return EXIT_SUCCESS;
}
