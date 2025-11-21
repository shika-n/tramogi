#include <algorithm>
#include <array>
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
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "macros.h"

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;

// TODO: Add validation layers

struct DeviceSuitableness {
	bool is_suitable = false;
	uint32_t graphics_queue_index = 0;
	uint32_t present_queue_index = 0;
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

	vk::raii::PipelineLayout pipeline_layout = nullptr;
	vk::raii::Pipeline graphics_pipeline = nullptr;

	vk::raii::CommandPool command_pool = nullptr;
	vk::raii::CommandBuffer command_buffer = nullptr;

	vk::raii::Semaphore present_complete_semaphore = nullptr;
	vk::raii::Semaphore render_finished_semaphore = nullptr;
	vk::raii::Fence draw_fence = nullptr;

	DeviceSuitableness device_suitableness {};

	std::vector<const char *> required_device_extensions = {
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

		window = glfwCreateWindow(WIDTH, HEIGHT, "Project Sky-High", nullptr, nullptr);
	}

	void init_vulkan() {
		create_instance();
		create_surface();
		pick_physical_device();
		create_logical_device();
		create_swapchain();
		create_image_views();
		create_graphics_pipeline();
		create_command_pool();
		create_command_buffer();
		create_sync_objects();
	}

	void main_loop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			draw_frame();
		}

		device.waitIdle();
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

		auto available_extensions = context.enumerateInstanceExtensionProperties();

		DLOG("Instance Extension:");
		for (uint32_t i = 0; i < extension_count; ++i) {
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
			.enabledExtensionCount = extension_count,
			.ppEnabledExtensionNames = extensions,
		};

		instance = vk::raii::Instance(context, create_info);
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

	void create_image_views() {
		swapchain_image_views.clear();

		vk::ImageViewCreateInfo image_view_create_info {
			.viewType = vk::ImageViewType::e2D,
			.format = swapchain_surface_format.format,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		for (const auto &image : swapchain_images) {
			image_view_create_info.image = image;
			swapchain_image_views.emplace_back(device, image_view_create_info);
		}
	}

	void create_graphics_pipeline() {
		auto shader_code = read_shader_file("shaders/slang.spv");
		DLOG("Shader size: {}", shader_code.size() * sizeof(char));

		vk::raii::ShaderModule shader_module = create_shader_module(shader_code);

		vk::PipelineShaderStageCreateInfo vertex_stage_create_info {
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = shader_module,
			.pName = "vertMain",
		};
		vk::PipelineShaderStageCreateInfo fragment_stage_create_info {
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = shader_module,
			.pName = "fragMain",
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

		vk::PipelineVertexInputStateCreateInfo vertex_input_info;
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
			.frontFace = vk::FrontFace::eClockwise,
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
		vk::PipelineColorBlendStateCreateInfo color_blending {
			.logicOpEnable = vk::False,
			.logicOp = vk::LogicOp::eCopy,
			.attachmentCount = 1,
			.pAttachments = &color_blend_attachment,
		};

		vk::PipelineLayoutCreateInfo pipeline_layout_info {
			.setLayoutCount = 0,
			.pushConstantRangeCount = 0,
		};

		pipeline_layout = vk::raii::PipelineLayout(device, pipeline_layout_info);

		vk::PipelineRenderingCreateInfo pipeline_rendering_info {
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &swapchain_surface_format.format,
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

	void create_command_buffer() {
		vk::CommandBufferAllocateInfo allocateInfo {
			.commandPool = command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		};

		command_buffer = std::move(vk::raii::CommandBuffers(device, allocateInfo).front());
	}

	void transition_image_layout(
		uint32_t image_index,
		vk::ImageLayout old_layout,
		vk::ImageLayout new_layout,
		vk::AccessFlags2 src_access_mask,
		vk::AccessFlags2 dst_access_mask,
		vk::PipelineStageFlags2 src_stage_mask,
		vk::PipelineStageFlags2 dst_stage_mask
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
			.image = swapchain_images[image_index],
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
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

		command_buffer.pipelineBarrier2(dependency_info);
	}

	void record_command_buffer(uint32_t image_index) {
		command_buffer.begin({});

		transition_image_layout(
			image_index,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput
		);

		vk::ClearValue clear_color = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::RenderingAttachmentInfo attachment_info {
			.imageView = swapchain_image_views[image_index],
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clear_color,
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
		};

		command_buffer.beginRendering(rendering_info);

		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline);
		command_buffer.setViewport(
			0,
			vk::Viewport(0.0f, 0.0f, swapchain_extent.width, swapchain_extent.height, 0.0f, 1.0f)
		);
		command_buffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchain_extent));

		command_buffer.draw(3, 1, 0, 0);

		command_buffer.endRendering();

		transition_image_layout(
			image_index,
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			{},
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eBottomOfPipe
		);

		command_buffer.end();
	}

	void create_sync_objects() {
		present_complete_semaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
		render_finished_semaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
		draw_fence = vk::raii::Fence(device, {.flags = vk::FenceCreateFlagBits::eSignaled});
	}

	void draw_frame() {
		device.waitIdle();

		auto [result, image_index] =
			swapchain.acquireNextImage(UINT64_MAX, *present_complete_semaphore, nullptr);

		record_command_buffer(image_index);
		device.resetFences(*draw_fence);

		vk::PipelineStageFlags wait_destination_stage_mask(
			vk::PipelineStageFlagBits::eColorAttachmentOutput
		);
		const vk::SubmitInfo submit_info {
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*present_complete_semaphore,
			.pWaitDstStageMask = &wait_destination_stage_mask,
			.commandBufferCount = 1,
			.pCommandBuffers = &*command_buffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &*render_finished_semaphore,
		};

		graphics_queue.submit(submit_info, *draw_fence);

		while (vk::Result::eTimeout == device.waitForFences(*draw_fence, vk::True, UINT64_MAX))
			;

		vk::PresentInfoKHR present_info {
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*render_finished_semaphore,
			.swapchainCount = 1,
			.pSwapchains = &*swapchain,
			.pImageIndices = &image_index,
		};

		result = present_queue.presentKHR(present_info);
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
