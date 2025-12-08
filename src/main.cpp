#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <format>
#include <fstream>
#include <functional>
#include <ios>
#include <limits>
#include <map>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/trigonometric.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "logging.h"

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;
const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

// TODO: Add validation layers
const std::array<const char *, 1> validation_layers = {
	"VK_LAYER_KHRONOS_validation",
};

#ifdef NDEBUG
constexpr bool enable_validation_layer = false;
#else
constexpr bool enable_validation_layer = true;
#endif

struct DeviceSuitableness {
	bool is_suitable = false;
	uint32_t graphics_queue_index = 0;
	uint32_t present_queue_index = 0;
};

struct Vertex {
	glm::vec3 position;
	glm::vec3 color;
	glm::vec2 tex_coord;

	bool operator==(const Vertex &other) const {
		return position == other.position && color == other.color && tex_coord == other.tex_coord;
	}

	static vk::VertexInputBindingDescription get_binding_description() {
		return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
	}

	static std::array<vk::VertexInputAttributeDescription, 3> get_attribute_description() {
		return {
			vk::VertexInputAttributeDescription {
				0,
				0,
				vk::Format::eR32G32B32Sfloat,
				offsetof(Vertex, position)
			},
			vk::VertexInputAttributeDescription {
				1,
				0,
				vk::Format::eR32G32B32Sfloat,
				offsetof(Vertex, color)
			},
			vk::VertexInputAttributeDescription {
				2,
				0,
				vk::Format::eR32G32Sfloat,
				offsetof(Vertex, tex_coord)
			}
		};
	}
};

namespace std {
template <> struct hash<Vertex> {
	size_t operator()(const Vertex &vertex) const {
		return ((hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec3>()(vertex.color) << 1)) >>
				1) ^
			   (hash<glm::vec2>()(vertex.tex_coord) << 1);
	}
};
} // namespace std

struct UniformBufferObject {
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 model;
};

class ProjectSkyHigh {
public:
	void run() {
		init_window();
		init_vulkan();
		main_loop();
		cleanup();
	}

private:
	GLFWwindow *window = nullptr;

	vk::raii::Context context {};
	vk::raii::Instance instance = nullptr;
	vk::raii::PhysicalDevice physical_device = nullptr;
	vk::raii::Device device = nullptr;
	vk::raii::Queue graphics_queue = nullptr;
	vk::raii::Queue present_queue = nullptr;
	vk::raii::SurfaceKHR surface = nullptr;

	vk::SurfaceFormatKHR swapchain_surface_format {};
	vk::Extent2D swapchain_extent {};
	vk::raii::SwapchainKHR swapchain = nullptr;
	std::vector<vk::Image> swapchain_images;
	std::vector<vk::raii::ImageView> swapchain_image_views;

	vk::raii::DescriptorSetLayout descriptor_set_layout = nullptr;
	vk::raii::PipelineLayout pipeline_layout = nullptr;
	vk::raii::Pipeline graphics_pipeline = nullptr;

	vk::raii::CommandPool command_pool = nullptr;
	std::vector<vk::raii::CommandBuffer> command_buffers;

	vk::raii::Buffer vertex_buffer = nullptr;
	vk::raii::DeviceMemory vertex_buffer_memory = nullptr;
	vk::raii::Buffer index_buffer = nullptr;
	vk::raii::DeviceMemory index_buffer_memory = nullptr;
	std::vector<vk::raii::Buffer> uniform_buffers;
	std::vector<vk::raii::DeviceMemory> uniform_buffers_memory;
	std::vector<void *> uniform_buffers_mapped;

	vk::raii::DescriptorPool descriptor_pool = nullptr;
	std::vector<vk::raii::DescriptorSet> descriptor_sets;

	std::vector<vk::raii::Semaphore> present_complete_semaphores;
	std::vector<vk::raii::Semaphore> render_finished_semaphores;
	std::vector<vk::raii::Fence> draw_fences;

	vk::raii::Image texture_image = nullptr;
	vk::raii::DeviceMemory texture_memory = nullptr;
	vk::raii::ImageView texture_image_view = nullptr;
	vk::raii::Sampler texture_sampler = nullptr;

	vk::raii::Image depth_image = nullptr;
	vk::raii::DeviceMemory depth_memory = nullptr;
	vk::raii::ImageView depth_image_view = nullptr;

	bool is_framebuffer_resized = false;

	vk::raii::DebugUtilsMessengerEXT debug_messenger = nullptr;

	DeviceSuitableness device_suitableness {};

	uint32_t current_frame = 0;

	std::vector<const char *> required_device_extensions = {
		vk::KHRSwapchainExtensionName,
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName,
		vk::KHRCreateRenderpass2ExtensionName,
	};

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	void init_window() {
		if (!glfwInit()) {
			throw std::runtime_error("Failed to initialize GLFW");
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Project Sky-High", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
	}

	void init_vulkan() {
		create_instance();
		setup_debug_messenger();
		create_surface();
		pick_physical_device();
		create_logical_device();
		create_swapchain();
		create_image_views();
		create_descriptor_layout();
		create_graphics_pipeline();
		create_command_pool();
		create_depth_resources();
		create_texture_image();
		create_texture_image_view();
		create_texture_sampler();
		load_model();
		create_vertex_buffer();
		create_index_buffer();
		create_uniform_buffers();
		create_descriptor_pool();
		create_descriptor_sets();
		create_command_buffers();
		create_sync_objects();
	}

	void main_loop() {
		auto last_time = std::chrono::high_resolution_clock().now();
		uint32_t frames = 0;
		double timer = 0;
		bool f3_pressed = false;
		bool print_fps = false;

		while (!glfwWindowShouldClose(window)) {
			auto now = std::chrono::high_resolution_clock().now();
			double delta =
				std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_time).count() /
				1000000000.0;

			glfwPollEvents();
			if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) {
				if (!f3_pressed) {
					print_fps = !print_fps;
					DLOG("Print FPS: {}", print_fps);
					f3_pressed = true;
				}
			} else {
				f3_pressed = false;
			}

			draw_frame();

			++frames;
			timer += delta;

			while (timer >= 1) {
				if (print_fps) {
					DLOG("{} FPS ({:.2f}ms)", frames, 1000.0 / frames);
				}
				frames = 0;
				timer -= 1;
			}

			last_time = now;
		}

		device.waitIdle();
	}

	void cleanup() {
		cleanup_swapchain();

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

		std::vector<const char *> required_layers;
		if (enable_validation_layer) {
			required_layers.assign(validation_layers.begin(), validation_layers.end());
		}

		auto layer_properties = context.enumerateInstanceLayerProperties();
		if (std::ranges::any_of(required_layers, [&layer_properties](const auto &required_layer) {
				return std::ranges::none_of(
					layer_properties,
					[required_layer](const auto &layer_property) {
						return strcmp(layer_property.layerName, required_layer) == 0;
					}
				);
			})) {
			throw std::runtime_error("One or more required validation layers are not supported");
		}

		auto extensions = get_required_extensions();

		auto available_extensions = context.enumerateInstanceExtensionProperties();

		DLOG("Instance Extension:");
		for (uint32_t i = 0; i < extensions.size(); ++i) {
			DLOG("  - {}", extensions[i]);
			if (std::ranges::none_of(
					available_extensions,
					[extension = extensions[i]](const auto &available_extension) {
						return std::strcmp(available_extension.extensionName, extension) == 0;
					}
				)) {
				throw std::runtime_error(
					"Required extension not supported: " + std::string(extensions[i])
				);
			}
		}

		vk::InstanceCreateInfo create_info {
			.pApplicationInfo = &app_info,
			.enabledLayerCount = static_cast<uint32_t>(required_layers.size()),
			.ppEnabledLayerNames = required_layers.data(),
			.enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
			.ppEnabledExtensionNames = extensions.data(),
		};

		instance = vk::raii::Instance(context, create_info);
	}

	std::vector<const char *> get_required_extensions() {
		uint32_t glfw_extension_count = 0;
		auto glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
		std::vector extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

		if (enable_validation_layer) {
			extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		return extensions;
	}

	void setup_debug_messenger() {
		if (!enable_validation_layer) {
			return;
		}

		vk::DebugUtilsMessageSeverityFlagsEXT severity_flags(
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
		);
		vk::DebugUtilsMessageTypeFlagsEXT message_type_flags(
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
		);

		vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_info {
			.messageSeverity = severity_flags,
			.messageType = message_type_flags,
			.pfnUserCallback = &debug_callback,
		};

		debug_messenger = instance.createDebugUtilsMessengerEXT(debug_messenger_info);
	}

	void create_surface() {
		VkSurfaceKHR surface_khr;
		if (glfwCreateWindowSurface(*instance, window, nullptr, &surface_khr) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create window surface");
		}
		surface = vk::raii::SurfaceKHR(instance, surface_khr);
	}

	DeviceSuitableness get_device_suitableness(vk::raii::PhysicalDevice device) {
		bool is_suitable = true;
		bool is_api_supported = true;
		bool anisotropy_support = true;
		std::map<const char *, bool> extension_support_map;

		auto property = device.getProperties();

		if (property.apiVersion < VK_API_VERSION_1_3) {
			is_suitable = false;
			is_api_supported = false;
		}

		auto available_extensions = device.enumerateDeviceExtensionProperties();
		for (const auto &extension : required_device_extensions) {
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

		auto supported_features = device.getFeatures();
		if (!supported_features.samplerAnisotropy) {
			is_suitable = false;
			anisotropy_support = false;
		}

		// TODO: Improve queue query
		auto queue_families = device.getQueueFamilyProperties();
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
				device.getSurfaceSupportKHR(i, *surface)) {
				present_queue_index = i;
			}
		}

		if (graphics_queue_index == queue_families.size() ||
			present_queue_index == queue_families.size()) {
			is_suitable = false;
		}

		DLOG("Device: {}", std::string(property.deviceName));
		DLOG("  Vulkan API v1.3 Support: {}", is_api_supported);
		DLOG("  Extensions:");
		for (auto entry : extension_support_map) {
			DLOG("    - {}: {}", entry.first, entry.second ? "Yes" : "No");
		}
		DLOG("  Anisotropy Support: {}", anisotropy_support);
		DLOG("  Queue:");
		DLOG(
			"    Graphics Queue Index: {}",
			graphics_queue_index == queue_families.size() ? "Not Found"
														  : std::to_string(graphics_queue_index)
		);
		DLOG(
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

	void pick_physical_device() {
		auto devices = instance.enumeratePhysicalDevices();
		if (devices.empty()) {
			throw std::runtime_error("No GPU that supports Vulkan found");
		}

		std::unordered_map<vk::raii::PhysicalDevice *, DeviceSuitableness> device_suitableness_map;
		std::ranges::for_each(devices, [this, &device_suitableness_map](auto &device) {
			device_suitableness_map[&device] = get_device_suitableness(device);
		});

		auto suitable_devices =
			std::ranges::filter_view(devices, [&device_suitableness_map](auto &device) {
				return device_suitableness_map[&device].is_suitable;
			});

		std::multimap<uint32_t, vk::raii::PhysicalDevice *, std::greater<uint32_t>> device_scores;
		for (auto &device : suitable_devices) {
			uint32_t score = get_device_score(device);
			device_scores.insert(std::make_pair(score, &device));
		}

		for (auto score_pair : device_scores) {
			if (score_pair.first > 0) {
				physical_device = *score_pair.second;
				device_suitableness = device_suitableness_map[score_pair.second];
				break;
			}
		}

		if (physical_device == nullptr) {
			throw std::runtime_error("No suitable device found");
		}

		DLOG("Using: {}", physical_device.getProperties().deviceName.data());
	}

	void create_logical_device() {
		float priority = 0.0f;
		vk::DeviceQueueCreateInfo device_queue_create_info {
			.queueFamilyIndex = device_suitableness.graphics_queue_index,
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
			.enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size()),
			.ppEnabledExtensionNames = required_device_extensions.data(),
		};

		device = vk::raii::Device(physical_device, device_create_info);
		graphics_queue = vk::raii::Queue(device, device_suitableness.graphics_queue_index, 0);
		present_queue = vk::raii::Queue(device, device_suitableness.present_queue_index, 0);
	}

	void create_swapchain() {
		auto surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(surface);
		std::vector<vk::SurfaceFormatKHR> available_formats = physical_device.getSurfaceFormatsKHR(
			surface
		);
		std::vector<vk::PresentModeKHR> available_present_mode =
			physical_device.getSurfacePresentModesKHR(surface);

		swapchain_surface_format = choose_swap_surface_format(available_formats);
		swapchain_extent = choose_swap_extent(surface_capabilities);
		auto min_image_count = std::max(3u, surface_capabilities.minImageCount);

		if (surface_capabilities.maxImageCount > 0 &&
			min_image_count > surface_capabilities.minImageCount) {
			min_image_count = surface_capabilities.maxImageCount;
		}

		uint32_t image_count = surface_capabilities.minImageCount + 1;
		if (surface_capabilities.maxImageCount > 0 &&
			image_count > surface_capabilities.maxImageCount) {
			image_count = surface_capabilities.maxImageCount;
		}

		vk::SwapchainCreateInfoKHR swapchain_create_info {
			.flags = vk::SwapchainCreateFlagsKHR(),
			.surface = surface,
			.minImageCount = min_image_count,
			.imageFormat = swapchain_surface_format.format,
			.imageColorSpace = swapchain_surface_format.colorSpace,
			.imageExtent = swapchain_extent,
			.imageArrayLayers = 1,
			.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
			.imageSharingMode = vk::SharingMode::eExclusive,
			.preTransform = surface_capabilities.currentTransform,
			.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
			.presentMode = choose_present_mode(available_present_mode),
			.clipped = true,
			.oldSwapchain = nullptr,
		};

		if (device_suitableness.graphics_queue_index != device_suitableness.present_queue_index) {
			std::array<uint32_t, 2> indices {
				device_suitableness.graphics_queue_index,
				device_suitableness.present_queue_index
			};

			swapchain_create_info.imageSharingMode = vk::SharingMode::eConcurrent;
			swapchain_create_info.queueFamilyIndexCount = 2;
			swapchain_create_info.pQueueFamilyIndices = indices.data();
		} else {
			swapchain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
			swapchain_create_info.queueFamilyIndexCount = 0;
			swapchain_create_info.pQueueFamilyIndices = nullptr;
		}

		swapchain = vk::raii::SwapchainKHR(device, swapchain_create_info);
		swapchain_images = swapchain.getImages();
	}

	vk::SurfaceFormatKHR choose_swap_surface_format(
		const std::vector<vk::SurfaceFormatKHR> &available_formats
	) {
		for (const auto &format : available_formats) {
			if (format.format == vk::Format::eB8G8R8A8Srgb &&
				format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
				return format;
			}
		}
		return available_formats[0];
	}

	vk::PresentModeKHR choose_present_mode(
		const std::vector<vk::PresentModeKHR> &available_present_mode
	) {
		for (const auto &mode : available_present_mode) {
			if (mode == vk::PresentModeKHR::eMailbox) {
				return mode;
			}
		}
		return vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR &capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}

		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window, &width, &height);

		return {
			std::clamp<uint32_t>(
				width,
				capabilities.minImageExtent.width,
				capabilities.maxImageExtent.width
			),
			std::clamp<uint32_t>(
				height,
				capabilities.minImageExtent.height,
				capabilities.maxImageExtent.height
			)
		};
	}

	vk::raii::ImageView create_image_view(
		vk::Image image,
		vk::Format format,
		vk::ImageAspectFlags aspect_flags
	) {
		vk::ImageViewCreateInfo view_info {
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = {
				.aspectMask = aspect_flags,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};
		return vk::raii::ImageView(device, view_info);
	}

	void create_image_views() {
		swapchain_image_views.clear();
		swapchain_image_views.reserve(swapchain_images.size());
		for (const auto &image : swapchain_images) {
			swapchain_image_views.emplace_back(create_image_view(
				image,
				swapchain_surface_format.format,
				vk::ImageAspectFlagBits::eColor
			));
		}
	}

	void create_descriptor_layout() {
		std::array bindings = {
			vk::DescriptorSetLayoutBinding {
				.binding = 0,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eVertex,
				.pImmutableSamplers = nullptr,
			},
			vk::DescriptorSetLayoutBinding {
				.binding = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment,
				.pImmutableSamplers = nullptr,
			}
		};

		vk::DescriptorSetLayoutCreateInfo layout_info {
			.bindingCount = bindings.size(),
			.pBindings = bindings.data(),
		};

		descriptor_set_layout = vk::raii::DescriptorSetLayout(device, layout_info);
	}

	void create_graphics_pipeline() {
		auto shader_code = read_shader_file("shaders/slang.spv");
		DLOG("Shader size: {}", shader_code.size() * sizeof(char));

		vk::raii::ShaderModule shader_module = create_shader_module(shader_code);

		vk::PipelineShaderStageCreateInfo vertex_stage_create_info {
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = shader_module,
			.pName = "vert_main",
		};
		vk::PipelineShaderStageCreateInfo fragment_stage_create_info {
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = shader_module,
			.pName = "frag_main",
		};

		std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages {
			vertex_stage_create_info,
			fragment_stage_create_info,
		};

		std::array<vk::DynamicState, 2> dynamic_states {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor,
		};
		vk::PipelineDynamicStateCreateInfo dynamic_state_create_info {
			.dynamicStateCount = dynamic_states.size(),
			.pDynamicStates = dynamic_states.data(),
		};

		auto binding_description = Vertex::get_binding_description();
		auto attribute_description = Vertex::get_attribute_description();
		vk::PipelineVertexInputStateCreateInfo vertex_input_info {
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &binding_description,
			.vertexAttributeDescriptionCount = attribute_description.size(),
			.pVertexAttributeDescriptions = attribute_description.data(),
		};
		vk::PipelineInputAssemblyStateCreateInfo input_assembly_info {
			.topology = vk::PrimitiveTopology::eTriangleList
		};

		vk::PipelineViewportStateCreateInfo viewport_state_info {
			.viewportCount = 1,
			.scissorCount = 1,
		};
		vk::PipelineRasterizationStateCreateInfo rasterization_state_info {
			.depthClampEnable = vk::False,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = vk::CullModeFlagBits::eBack,
			.frontFace = vk::FrontFace::eCounterClockwise,
			.depthBiasEnable = vk::False,
			.depthBiasSlopeFactor = 1,
			.lineWidth = 1,
		};
		vk::PipelineMultisampleStateCreateInfo multisample_info {
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
			.sampleShadingEnable = vk::False,
		};
		vk::PipelineColorBlendAttachmentState color_blend_attachment {
			.blendEnable = vk::False,
			.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
							  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
		};
		vk::PipelineDepthStencilStateCreateInfo depth_stencil_info {
			.depthTestEnable = vk::True,
			.depthWriteEnable = vk::True,
			.depthCompareOp = vk::CompareOp::eLess,
			.depthBoundsTestEnable = vk::False,
			.stencilTestEnable = vk::False,
		};
		vk::PipelineColorBlendStateCreateInfo color_blending {
			.logicOpEnable = vk::False,
			.logicOp = vk::LogicOp::eCopy,
			.attachmentCount = 1,
			.pAttachments = &color_blend_attachment,
		};

		vk::PipelineLayoutCreateInfo pipeline_layout_info {
			.setLayoutCount = 1,
			.pSetLayouts = &*descriptor_set_layout,
			.pushConstantRangeCount = 0,
		};

		pipeline_layout = vk::raii::PipelineLayout(device, pipeline_layout_info);

		auto depth_format = find_depth_format();

		vk::PipelineRenderingCreateInfo pipeline_rendering_info {
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &swapchain_surface_format.format,
			.depthAttachmentFormat = depth_format,
		};

		vk::GraphicsPipelineCreateInfo graphics_pipeline_info {
			.pNext = &pipeline_rendering_info,
			.stageCount = 2,
			.pStages = shader_stages.data(),
			.pVertexInputState = &vertex_input_info,
			.pInputAssemblyState = &input_assembly_info,
			.pViewportState = &viewport_state_info,
			.pRasterizationState = &rasterization_state_info,
			.pMultisampleState = &multisample_info,
			.pDepthStencilState = &depth_stencil_info,
			.pColorBlendState = &color_blending,
			.pDynamicState = &dynamic_state_create_info,
			.layout = pipeline_layout,
			.renderPass = nullptr,
		};

		graphics_pipeline = vk::raii::Pipeline(device, nullptr, graphics_pipeline_info);
	}

	[[nodiscard]] vk::raii::ShaderModule create_shader_module(const std::vector<char> &code) const {
		vk::ShaderModuleCreateInfo shader_module_create_info {
			.codeSize = code.size() * sizeof(char),
			.pCode = reinterpret_cast<const uint32_t *>(code.data()),
		};

		return vk::raii::ShaderModule(device, shader_module_create_info);
	}

	void create_command_pool() {
		vk::CommandPoolCreateInfo pool_info {
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = device_suitableness.graphics_queue_index,
		};

		command_pool = vk::raii::CommandPool(device, pool_info);
	}

	vk::Format find_supported_format(
		const std::vector<vk::Format> &formats,
		vk::ImageTiling tiling,
		vk::FormatFeatureFlags features
	) {
		for (const auto format : formats) {
			vk::FormatProperties properties = physical_device.getFormatProperties(format);
			if (tiling == vk::ImageTiling::eLinear &&
				(properties.linearTilingFeatures & features) == features) {
				return format;
			}
			if (tiling == vk::ImageTiling::eOptimal &&
				(properties.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		throw std::runtime_error("Failed to find supported format");
	}

	vk::Format find_depth_format() {
		return find_supported_format(
			{
				vk::Format::eD32Sfloat,
				vk::Format::eD32SfloatS8Uint,
				vk::Format::eD24UnormS8Uint,
			},
			vk::ImageTiling::eOptimal,
			vk::FormatFeatureFlagBits::eDepthStencilAttachment
		);
	}

	bool has_stencil_component(vk::Format format) {
		return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
	}

	void create_depth_resources() {
		vk::Format depth_format = find_depth_format();
		create_image(
			swapchain_extent.width,
			swapchain_extent.height,
			depth_format,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eDepthStencilAttachment,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			depth_image,
			depth_memory
		);
		depth_image_view =
			create_image_view(depth_image, depth_format, vk::ImageAspectFlagBits::eDepth);
	}

	void create_texture_image() {
		int texture_width = 0;
		int texture_height = 0;
		int texture_channels = 0;
		stbi_uc *pixels = stbi_load(
			TEXTURE_PATH.c_str(),
			&texture_width,
			&texture_height,
			&texture_channels,
			STBI_rgb_alpha
		);

		vk::DeviceSize image_size = texture_width * texture_height * 4;

		if (!pixels) {
			// TODO: handle missing texture without throwing
			throw std::runtime_error("Failed to load texture image");
		}

		vk::raii::Buffer staging_buffer({});
		vk::raii::DeviceMemory staging_memory({});
		create_buffer(
			image_size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			staging_buffer,
			staging_memory
		);

		void *data = staging_memory.mapMemory(0, image_size);
		memcpy(data, pixels, image_size);
		staging_memory.unmapMemory();
		data = nullptr;

		stbi_image_free(pixels);
		pixels = nullptr;

		create_image(
			texture_width,
			texture_height,
			vk::Format::eR8G8B8A8Srgb,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			texture_image,
			texture_memory
		);

		transition_image_layout(
			texture_image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal
		);
		copy_buffer_to_image(staging_buffer, texture_image, texture_width, texture_height);
		transition_image_layout(
			texture_image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal
		);
	}

	void create_texture_image_view() {
		texture_image_view = create_image_view(
			texture_image,
			vk::Format::eR8G8B8A8Srgb,
			vk::ImageAspectFlagBits::eColor
		);
	}

	void create_texture_sampler() {
		vk::PhysicalDeviceProperties properties = physical_device.getProperties();
		vk::SamplerCreateInfo sampler_info {
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eLinear,
			.addressModeU = vk::SamplerAddressMode::eRepeat,
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.addressModeW = vk::SamplerAddressMode::eRepeat,
			.anisotropyEnable = vk::True,
			.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
			.compareEnable = vk::False,
			.compareOp = vk::CompareOp::eAlways,
		};
		texture_sampler = vk::raii::Sampler(device, sampler_info);
	}

	void create_image(
		uint32_t width,
		uint32_t height,
		vk::Format format,
		vk::ImageTiling tiling,
		vk::ImageUsageFlags usage,
		vk::MemoryPropertyFlags properties,
		vk::raii::Image &image,
		vk::raii::DeviceMemory &image_memory
	) {
		vk::ImageCreateInfo image_info {
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = {width, height, 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = tiling,
			.usage = usage,
			.sharingMode = vk::SharingMode::eExclusive,
		};

		image = vk::raii::Image(device, image_info);

		vk::MemoryRequirements memory_requirements = image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocate_info {
			.allocationSize = memory_requirements.size,
			.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties),
		};

		image_memory = vk::raii::DeviceMemory(device, allocate_info);
		image.bindMemory(image_memory, 0);
	}

	vk::raii::CommandBuffer begin_single_time_commands() {
		vk::CommandBufferAllocateInfo allocate_info {
			.commandPool = command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		};

		vk::raii::CommandBuffer command_buffer = std::move(
			device.allocateCommandBuffers(allocate_info).front()
		);

		vk::CommandBufferBeginInfo begin_info {
			.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
		};
		command_buffer.begin(begin_info);

		return command_buffer;
	}

	void end_single_time_commands(vk::raii::CommandBuffer &command_buffer) {
		command_buffer.end();

		vk::SubmitInfo submit_info {
			.commandBufferCount = 1,
			.pCommandBuffers = &*command_buffer,
		};

		graphics_queue.submit(submit_info, nullptr);
		graphics_queue.waitIdle();
	}

	void create_command_buffers() {
		vk::CommandBufferAllocateInfo allocateInfo {
			.commandPool = command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = MAX_FRAMES_IN_FLIGHT,
		};

		command_buffers = vk::raii::CommandBuffers(device, allocateInfo);
	}

	void create_buffer(
		vk::DeviceSize size,
		vk::BufferUsageFlags usage,
		vk::MemoryPropertyFlags memory_properties,
		vk::raii::Buffer &buffer,
		vk::raii::DeviceMemory &buffer_memory
	) {
		vk::BufferCreateInfo buffer_info {
			.size = size,
			.usage = usage,
			.sharingMode = vk::SharingMode::eExclusive,
		};

		buffer = vk::raii::Buffer(device, buffer_info);

		auto memory_requirements = buffer.getMemoryRequirements();
		vk::MemoryAllocateInfo memory_allocate_info {
			.allocationSize = memory_requirements.size,
			.memoryTypeIndex =
				find_memory_type(memory_requirements.memoryTypeBits, memory_properties)
		};

		buffer_memory = vk::raii::DeviceMemory(device, memory_allocate_info);

		buffer.bindMemory(buffer_memory, 0);
	}

	void load_model() {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::unordered_map<Vertex, uint32_t> unique_vertices;
		std::string warn;
		std::string err;

		if (tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
			DLOG("{}", warn + err);
		}

		for (auto shape : shapes) {
			for (const auto &index : shape.mesh.indices) {
				Vertex vertex {
					.position =
						{
							attrib.vertices[3 * index.vertex_index + 0],
							attrib.vertices[3 * index.vertex_index + 1],
							attrib.vertices[3 * index.vertex_index + 2],
						},
					.color = {1.0f, 1.0f, 1.0f},
					.tex_coord = {
						attrib.texcoords[2 * index.texcoord_index + 0],
						1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
					},
				};

				if (unique_vertices.count(vertex) == 0) {
					unique_vertices[vertex] = static_cast<glm::uint>(vertices.size());
					vertices.push_back(vertex);
				}

				indices.push_back(unique_vertices[vertex]);
			}
		}

		DLOG("Loading model done!");
		DLOG("  Vertices: {}", vertices.size());
		DLOG("  Indices: {}", indices.size());
	}

	void create_vertex_buffer() {
		auto buffer_size = sizeof(vertices[0]) * vertices.size();

		vk::raii::Buffer staging_buffer = nullptr;
		vk::raii::DeviceMemory staging_buffer_memory = nullptr;
		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			staging_buffer,
			staging_buffer_memory
		);
		void *staging_data = staging_buffer_memory.mapMemory(0, buffer_size);
		memcpy(staging_data, vertices.data(), buffer_size);
		staging_buffer_memory.unmapMemory();
		staging_data = nullptr;

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vertex_buffer,
			vertex_buffer_memory
		);

		copy_buffer(staging_buffer, vertex_buffer, buffer_size);
	}

	void create_index_buffer() {
		auto buffer_size = sizeof(indices[0]) * indices.size();

		vk::raii::Buffer staging_buffer = nullptr;
		vk::raii::DeviceMemory staging_buffer_memory = nullptr;
		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			staging_buffer,
			staging_buffer_memory
		);
		void *staging_data = staging_buffer_memory.mapMemory(0, buffer_size);
		memcpy(staging_data, indices.data(), buffer_size);
		staging_buffer_memory.unmapMemory();
		staging_data = nullptr;

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			index_buffer,
			index_buffer_memory
		);

		copy_buffer(staging_buffer, index_buffer, buffer_size);
	}

	void create_uniform_buffers() {
		uniform_buffers.clear();
		uniform_buffers_memory.clear();
		uniform_buffers_mapped.clear();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vk::DeviceSize buffer_size = sizeof(UniformBufferObject);
			vk::raii::Buffer buffer({});
			vk::raii::DeviceMemory buffer_memory({});
			create_buffer(
				buffer_size,
				vk::BufferUsageFlagBits::eUniformBuffer,
				vk::MemoryPropertyFlagBits::eHostVisible |
					vk::MemoryPropertyFlagBits::eHostCoherent,
				buffer,
				buffer_memory
			);

			uniform_buffers.emplace_back(std::move(buffer));
			uniform_buffers_mapped.emplace_back(buffer_memory.mapMemory(0, buffer_size));
			uniform_buffers_memory.emplace_back(std::move(buffer_memory));
		}
	}

	void create_descriptor_pool() {
		std::array pool_sizes {
			vk::DescriptorPoolSize {
				.type = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = MAX_FRAMES_IN_FLIGHT,
			},
			vk::DescriptorPoolSize {
				.type = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = MAX_FRAMES_IN_FLIGHT,
			},
		};

		vk::DescriptorPoolCreateInfo pool_info {
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = MAX_FRAMES_IN_FLIGHT,
			.poolSizeCount = pool_sizes.size(),
			.pPoolSizes = pool_sizes.data(),
		};

		descriptor_pool = vk::raii::DescriptorPool(device, pool_info);
	}

	void create_descriptor_sets() {
		descriptor_sets.clear();

		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptor_set_layout);
		vk::DescriptorSetAllocateInfo allocate_info {
			.descriptorPool = descriptor_pool,
			.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
			.pSetLayouts = layouts.data(),
		};

		descriptor_sets = device.allocateDescriptorSets(allocate_info);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vk::DescriptorBufferInfo buffer_info {
				.buffer = uniform_buffers[i],
				.offset = 0,
				.range = sizeof(UniformBufferObject),
			};
			vk::DescriptorImageInfo image_info {
				.sampler = texture_sampler,
				.imageView = texture_image_view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			};
			std::array descriptor_writes {
				vk::WriteDescriptorSet {
					.dstSet = descriptor_sets[i],
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = &buffer_info,
				},
				vk::WriteDescriptorSet {
					.dstSet = descriptor_sets[i],
					.dstBinding = 1,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eCombinedImageSampler,
					.pImageInfo = &image_info,
				},
			};
			device.updateDescriptorSets(descriptor_writes, {});
		}
	}

	void copy_buffer(vk::raii::Buffer &src, vk::raii::Buffer &dst, vk::DeviceSize size) {
		vk::raii::CommandBuffer command_copy_buffer = begin_single_time_commands();
		command_copy_buffer.copyBuffer(src, dst, vk::BufferCopy {0, 0, size});
		end_single_time_commands(command_copy_buffer);
	}

	void copy_buffer_to_image(
		const vk::raii::Buffer &buffer,
		vk::raii::Image &image,
		uint32_t width,
		uint32_t height
	) {
		vk::raii::CommandBuffer command_buffer = begin_single_time_commands();

		vk::BufferImageCopy region {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
			.imageOffset = {0, 0, 0},
			.imageExtent = {width, height, 1},
		};

		command_buffer
			.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

		end_single_time_commands(command_buffer);
	}

	uint32_t find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) {
		auto memory_properties = physical_device.getMemoryProperties();
		for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
			if (type_filter & (1 << i) &&
				(memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("Failed to find suitable memory type");
	}

	void transition_image_layout(
		const vk::raii::Image &image,
		vk::ImageLayout old_layout,
		vk::ImageLayout new_layout
	) {
		auto command_buffer = begin_single_time_commands();

		vk::PipelineStageFlags source_stage;
		vk::PipelineStageFlags destination_stage;

		vk::ImageMemoryBarrier barrier {
			.oldLayout = old_layout,
			.newLayout = new_layout,
			.image = image,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};

		if (old_layout == vk::ImageLayout::eUndefined &&
			new_layout == vk::ImageLayout::eTransferDstOptimal) {
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
			destination_stage = vk::PipelineStageFlagBits::eTransfer;
		} else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
				   new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			source_stage = vk::PipelineStageFlagBits::eTransfer;
			destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
		} else {
			throw std::invalid_argument("Unsupported layout transition");
		}

		command_buffer.pipelineBarrier(source_stage, destination_stage, {}, {}, nullptr, barrier);

		end_single_time_commands(command_buffer);
	}

	void transition_image_layout(
		vk::Image image,
		vk::ImageLayout old_layout,
		vk::ImageLayout new_layout,
		vk::AccessFlags2 src_access_mask,
		vk::AccessFlags2 dst_access_mask,
		vk::PipelineStageFlags2 src_stage_mask,
		vk::PipelineStageFlags2 dst_stage_mask,
		vk::ImageAspectFlags aspect_flags
	) {
		vk::ImageMemoryBarrier2 barrier = {
			.srcStageMask = src_stage_mask,
			.srcAccessMask = src_access_mask,
			.dstStageMask = dst_stage_mask,
			.dstAccessMask = dst_access_mask,
			.oldLayout = old_layout,
			.newLayout = new_layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = {
				.aspectMask = aspect_flags,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};

		vk::DependencyInfo dependency_info {
			.dependencyFlags = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier,
		};

		command_buffers[current_frame].pipelineBarrier2(dependency_info);
	}

	void record_command_buffer(uint32_t image_index) {
		command_buffers[current_frame].begin({});

		transition_image_layout(
			swapchain_images[image_index],
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::ImageAspectFlagBits::eColor
		);
		transition_image_layout(
			*depth_image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests |
				vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests |
				vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth
		);

		vk::ClearValue clear_color = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue clear_depth = vk::ClearDepthStencilValue(1.0f, 0);
		vk::RenderingAttachmentInfo attachment_info {
			.imageView = swapchain_image_views[image_index],
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clear_color,
		};
		vk::RenderingAttachmentInfo depth_attachment_info {
			.imageView = depth_image_view,
			.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = clear_depth,
		};

		vk::RenderingInfo rendering_info {
			.renderArea =
				{
					.offset = {0, 0},
					.extent = swapchain_extent,
				},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &attachment_info,
			.pDepthAttachment = &depth_attachment_info,
		};

		command_buffers[current_frame].beginRendering(rendering_info);

		command_buffers[current_frame].bindPipeline(
			vk::PipelineBindPoint::eGraphics,
			graphics_pipeline
		);
		command_buffers[current_frame].setViewport(
			0,
			vk::Viewport(0.0f, 0.0f, swapchain_extent.width, swapchain_extent.height, 0.0f, 1.0f)
		);
		command_buffers[current_frame].setScissor(
			0,
			vk::Rect2D(vk::Offset2D(0, 0), swapchain_extent)
		);

		command_buffers[current_frame].bindVertexBuffers(0, *vertex_buffer, {0});
		command_buffers[current_frame].bindIndexBuffer(index_buffer, 0, vk::IndexType::eUint32);

		command_buffers[current_frame].bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pipeline_layout,
			0,
			*descriptor_sets[current_frame],
			nullptr
		);

		// command_buffers[current_frame].draw(3, 1, 1, 0);
		command_buffers[current_frame].drawIndexed(indices.size(), 1, 0, 0, 0);
		command_buffers[current_frame].endRendering();

		transition_image_layout(
			swapchain_images[image_index],
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			{},
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eBottomOfPipe,
			vk::ImageAspectFlagBits::eColor
		);

		command_buffers[current_frame].end();
	}

	void create_sync_objects() {
		present_complete_semaphores.clear();
		render_finished_semaphores.clear();
		draw_fences.clear();

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			present_complete_semaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			render_finished_semaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			draw_fences.emplace_back(
				device,
				vk::FenceCreateInfo {.flags = vk::FenceCreateFlagBits::eSignaled}
			);
		}
	}

	void draw_frame() {
		device.waitIdle();
		while (vk::Result::eTimeout ==
			   device.waitForFences(*draw_fences[current_frame], vk::True, UINT64_MAX))
			;

		try {
			auto [result, image_index] = swapchain.acquireNextImage(
				UINT64_MAX,
				*present_complete_semaphores[current_frame],
				nullptr
			);

			if (result == vk::Result::eErrorOutOfDateKHR) {
				recreate_swapchain();
				return;
			}

			if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
				throw std::runtime_error("Failed to acquire swapchain image");
			}

			command_buffers[current_frame].reset();
			record_command_buffer(image_index);
			device.resetFences(*draw_fences[current_frame]);

			update_uniform_buffer(current_frame);

			vk::PipelineStageFlags wait_destination_stage_mask(
				vk::PipelineStageFlagBits::eColorAttachmentOutput
			);
			const vk::SubmitInfo submit_info {
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &*present_complete_semaphores[current_frame],
				.pWaitDstStageMask = &wait_destination_stage_mask,
				.commandBufferCount = 1,
				.pCommandBuffers = &*command_buffers[current_frame],
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &*render_finished_semaphores[current_frame],
			};

			graphics_queue.submit(submit_info, draw_fences[current_frame]);

			vk::PresentInfoKHR present_info {
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &*render_finished_semaphores[current_frame],
				.swapchainCount = 1,
				.pSwapchains = &*swapchain,
				.pImageIndices = &image_index,
			};

			result = present_queue.presentKHR(present_info);
			if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR ||
				is_framebuffer_resized) {
				is_framebuffer_resized = false;
				recreate_swapchain();
			} else if (result != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to present swapchain image");
			}
		} catch (const vk::SystemError &e) {
			if (e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR)) {
				recreate_swapchain();
				return;
			} else {
				throw e;
			}
		}

		current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void update_uniform_buffer(uint32_t current_image) {
		static auto start_time = std::chrono::high_resolution_clock::now();

		auto current_time = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(
						 current_time - start_time
		)
						 .count();

		UniformBufferObject ubo;
		ubo.model =
			glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(
			glm::vec3(2.0f, 2.0f, 2.0f),
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f)
		);
		ubo.projection = glm::perspective(
			glm::radians(45.0f),
			static_cast<float>(swapchain_extent.width) /
				static_cast<float>(swapchain_extent.height),
			0.1f,
			10.0f
		);
		ubo.projection[1][1] *= -1;

		memcpy(uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
	}

	void cleanup_swapchain() {
		swapchain_images.clear();
		swapchain = nullptr;
	}

	void recreate_swapchain() {
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		device.waitIdle();
		while (vk::Result::eTimeout ==
			   device.waitForFences(*draw_fences[current_frame], vk::True, UINT64_MAX))
			;

		cleanup_swapchain();

		create_swapchain();
		create_image_views();
		create_depth_resources();

		DLOG("Resized to: {}x{}", width, height);
	}

	static std::vector<char> read_shader_file(const char *filepath) {
		std::ifstream file(filepath, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open shader file");
		}

		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

		file.close();

		return buffer;
	}

	static void framebuffer_resize_callback(GLFWwindow *window, int, int) {
		auto app = reinterpret_cast<ProjectSkyHigh *>(glfwGetWindowUserPointer(window));
		app->is_framebuffer_resized = true;
	}

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
		vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
		vk::DebugUtilsMessageTypeFlagsEXT type,
		const vk::DebugUtilsMessengerCallbackDataEXT *p_callback_data,
		void *
	) {
		std::println(
			stderr,
			"Validation layer: type {} [{}] message: {}",
			vk::to_string(severity),
			vk::to_string(type),
			p_callback_data->pMessage
		);
		return vk::False;
	}
};

int main() {
	DLOG("Running in DEBUG mode");

	ProjectSkyHigh skyhigh;

	try {
		skyhigh.run();
	} catch (const vk::SystemError &e) {
		std::println(stderr, "Vulkan Error: {}", e.what());
		return EXIT_FAILURE;
	} catch (const std::exception &e) {
		std::println(stderr, "Error: {}", e.what());
		return EXIT_FAILURE;
	}

	DLOG("Exited successfully");
	return EXIT_SUCCESS;
}
