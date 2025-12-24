#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <expected>
#include <format>
#include <functional>
#include <limits>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/trigonometric.hpp>

#include "graphics/allocator.h"
#include "graphics/device.h"
#include "graphics/dispatch_loader.h"
#include "graphics/instance.h"
#include "graphics/physical_device.h"
#include "graphics/surface.h"
#include "tramogi/core/io/file.h"
#include "tramogi/core/io/image_data.h"
#include "tramogi/core/io/model.h"
#include "tramogi/core/logging/logging.h"
#include "tramogi/graphics/buffer.h"
#include "tramogi/input/keyboard.h"
#include "tramogi/platform/window.h"

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;
const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

using namespace tramogi::core;
using namespace tramogi::platform;

using namespace tramogi::core::logging;

static vk::VertexInputBindingDescription get_binding_description() {
	return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
}

static std::array<vk::VertexInputAttributeDescription, 2> get_attribute_description() {
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
			vk::Format::eR32G32Sfloat,
			offsetof(Vertex, tex_coord)
		}
	};
}

struct UniformBufferObject {
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 model;
};

class ProjectSkyHigh {
public:
	ProjectSkyHigh() : device(physical_device) {}

	void run() {
		init_window();
		init_vulkan();
		main_loop();
		cleanup();
	}

private:
	Window window;

	tramogi::graphics::Instance instance;
	tramogi::graphics::PhysicalDevice physical_device;
	tramogi::graphics::Device device;

	vk::SurfaceFormatKHR swapchain_surface_format {};
	vk::Extent2D swapchain_extent {};
	vk::raii::SwapchainKHR swapchain = nullptr;
	std::vector<vk::Image> swapchain_images;
	std::vector<vk::raii::ImageView> swapchain_image_views;

	vk::raii::DescriptorSetLayout descriptor_set_layout = nullptr;
	vk::raii::PipelineLayout pipeline_layout = nullptr;
	vk::raii::Pipeline graphics_pipeline = nullptr;

	std::vector<vk::raii::CommandBuffer> command_buffers;

	tramogi::graphics::VertexBuffer vertex_buffer;
	tramogi::graphics::IndexBuffer index_buffer;
	std::vector<tramogi::graphics::UniformBuffer> uniform_buffers;

	vk::raii::DescriptorPool descriptor_pool = nullptr;
	std::vector<vk::raii::DescriptorSet> descriptor_sets;

	uint32_t mip_levels = 0;
	vk::raii::Image texture_image = nullptr;
	vk::raii::DeviceMemory texture_memory = nullptr;
	vk::raii::ImageView texture_image_view = nullptr;
	vk::raii::Sampler texture_sampler = nullptr;

	vk::raii::Image depth_image = nullptr;
	vk::raii::DeviceMemory depth_memory = nullptr;
	vk::raii::ImageView depth_image_view = nullptr;

	uint32_t current_frame = 0;

	tramogi::core::Model model;

	tramogi::input::Keyboard input;

	void init_window() {
		if (!window.init(WIDTH, HEIGHT, "Tramogi Demo")) {
			throw std::runtime_error("Failed to initialize GLFW");
		}
		window.set_key_callback([this](int scancode, bool is_pressed) {
			input.set_key(scancode, is_pressed);
		});
	}

	void init_vulkan() {
		create_instance();
		pick_physical_device();
		create_logical_device();

		create_swapchain();
		create_image_views();
		create_descriptor_layout();
		create_graphics_pipeline();
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
	}

	void main_loop() {
		auto last_time = std::chrono::high_resolution_clock().now();
		uint32_t frames = 0;
		double timer = 0;
		bool print_fps = false;

		while (!window.should_close()) {
			auto now = std::chrono::high_resolution_clock().now();
			double delta =
				std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_time).count() /
				1000000000.0;

			window.poll_events();
			if (input.is_pressed(tramogi::input::Key::P)) {
				print_fps = !print_fps;
				debug_log("Print FPS: {}", print_fps);
				input.consume_key(tramogi::input::Key::P);
			}
			if (input.is_pressed(tramogi::input::Key::Q)) {
				window.request_close();
			}

			draw_frame(delta);

			++frames;
			timer += delta;

			while (timer >= 1) {
				if (print_fps) {
					debug_log("{} FPS ({:.2f}ms)", frames, 1000.0 / frames);
				}
				frames = 0;
				timer -= 1;
			}

			last_time = now;
		}

		device.wait_idle(current_frame);
	}

	void cleanup() {
		cleanup_swapchain();
	}

	void create_instance() {
		auto extensions = window.get_required_extensions();
		auto result = instance.init(extensions);
		if (!result) {
			throw std::runtime_error(result.error());
		}
	}

	void pick_physical_device() {
		Result<vk::SurfaceKHR> surface_result = window.create_surface(instance.get_instance());
		if (!surface_result) {
			throw std::runtime_error(std::string(surface_result.error()));
		}

		auto result = physical_device.init(instance, surface_result.value());
		if (!result) {
			throw std::runtime_error(result.error());
		}
	}

	void create_logical_device() {
		device.init(instance);
	}

	void create_swapchain() {
		auto surface_capabilities = physical_device.get_surface_capabilities();
		std::vector<vk::SurfaceFormatKHR> available_formats = physical_device.get_surface_formats();
		std::vector<vk::PresentModeKHR> available_present_mode =
			physical_device.get_surface_present_modes();

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
			.surface = physical_device.get_surface(),
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
			.clipped = vk::True,
			.oldSwapchain = nullptr,
		};

		if (physical_device.get_graphics_queue_index() !=
			physical_device.get_present_queue_index()) {
			std::array<uint32_t, 2> indices {
				physical_device.get_graphics_queue_index(),
				physical_device.get_present_queue_index()
			};

			swapchain_create_info.imageSharingMode = vk::SharingMode::eConcurrent;
			swapchain_create_info.queueFamilyIndexCount = 2;
			swapchain_create_info.pQueueFamilyIndices = indices.data();
		} else {
			swapchain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
			swapchain_create_info.queueFamilyIndexCount = 0;
			swapchain_create_info.pQueueFamilyIndices = nullptr;
		}

		swapchain = vk::raii::SwapchainKHR(device.get_device(), swapchain_create_info);
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

		Dimension dimension = window.get_size();

		return {
			std::clamp<uint32_t>(
				dimension.width,
				capabilities.minImageExtent.width,
				capabilities.maxImageExtent.width
			),
			std::clamp<uint32_t>(
				dimension.height,
				capabilities.minImageExtent.height,
				capabilities.maxImageExtent.height
			)
		};
	}

	vk::raii::ImageView create_image_view(
		vk::Image image,
		vk::Format format,
		vk::ImageAspectFlags aspect_flags,
		uint32_t mip_levels
	) {
		vk::ImageViewCreateInfo view_info {
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = {
				.aspectMask = aspect_flags,
				.baseMipLevel = 0,
				.levelCount = mip_levels,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};
		return vk::raii::ImageView(device.get_device(), view_info);
	}

	void create_image_views() {
		swapchain_image_views.clear();
		swapchain_image_views.reserve(swapchain_images.size());
		for (const auto &image : swapchain_images) {
			swapchain_image_views.emplace_back(create_image_view(
				image,
				swapchain_surface_format.format,
				vk::ImageAspectFlagBits::eColor,
				1
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

		descriptor_set_layout = vk::raii::DescriptorSetLayout(device.get_device(), layout_info);
	}

	void create_graphics_pipeline() {
		auto shader_code_result = read_shader_file("shaders/slang.spv");
		if (!shader_code_result) {
			throw std::runtime_error(shader_code_result.error());
		}
		auto shader_code = shader_code_result.value();

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

		auto binding_description = get_binding_description();
		auto attribute_description = get_attribute_description();
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

		pipeline_layout = vk::raii::PipelineLayout(device.get_device(), pipeline_layout_info);

		auto depth_format = physical_device.get_depth_format();
		if (!depth_format) {
			throw std::runtime_error(depth_format.error());
		}

		vk::PipelineRenderingCreateInfo pipeline_rendering_info {
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &swapchain_surface_format.format,
			.depthAttachmentFormat = depth_format.value(),
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

		graphics_pipeline =
			vk::raii::Pipeline(device.get_device(), nullptr, graphics_pipeline_info);
	}

	[[nodiscard]] vk::raii::ShaderModule create_shader_module(const std::vector<char> &code) const {
		vk::ShaderModuleCreateInfo shader_module_create_info {
			.codeSize = code.size() * sizeof(char),
			.pCode = reinterpret_cast<const uint32_t *>(code.data()),
		};

		return vk::raii::ShaderModule(device.get_device(), shader_module_create_info);
	}

	bool has_stencil_component(vk::Format format) {
		return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
	}

	void create_depth_resources() {
		Result<vk::Format> depth_format = physical_device.get_depth_format();
		if (!depth_format) {
			throw std::runtime_error(depth_format.error());
		}
		create_image(
			swapchain_extent.width,
			swapchain_extent.height,
			1,
			depth_format.value(),
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eDepthStencilAttachment,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			depth_image,
			depth_memory
		);
		depth_image_view = create_image_view(
			depth_image,
			depth_format.value(),
			vk::ImageAspectFlagBits::eDepth,
			1
		);
	}

	void create_texture_image() {
		tramogi::core::ImageData image_data;
		if (!image_data.load_from_file(TEXTURE_PATH.c_str())) {
			// TODO: handle missing texture without throwing
			throw std::runtime_error("Failed to load texture image");
		}

		int texture_width = image_data.get_width();
		int texture_height = image_data.get_height();
		mip_levels = image_data.get_mip_levels();
		vk::DeviceSize image_size = image_data.get_size();

		tramogi::graphics::StagingBuffer staging_buffer;
		auto result = staging_buffer.init(device, image_size);
		if (!result) {
			throw std::runtime_error(result.error());
		}

		staging_buffer.map();
		staging_buffer.upload_data(image_data.get_data());
		staging_buffer.unmap();

		create_image(
			texture_width,
			texture_height,
			mip_levels,
			vk::Format::eR8G8B8A8Srgb,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
				vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			texture_image,
			texture_memory
		);

		transition_image_layout(
			texture_image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			mip_levels
		);
		copy_buffer_to_image(
			staging_buffer.get_buffer(),
			texture_image,
			texture_width,
			texture_height
		);

		generate_mipmaps(texture_image, texture_width, texture_height, mip_levels);
	}

	void generate_mipmaps(
		vk::Image image,
		int32_t texture_width,
		int32_t texture_height,
		uint32_t mip_levels
	) {
		vk::raii::CommandBuffer command_buffer = begin_single_time_commands();

		vk::ImageMemoryBarrier barrier {
			.srcAccessMask = vk::AccessFlagBits::eTransferWrite,
			.dstAccessMask = vk::AccessFlagBits::eTransferRead,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eTransferSrcOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = image,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		int32_t mip_width = texture_width;
		int32_t mip_height = texture_height;

		for (uint32_t i = 1; i < mip_levels; ++i) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

			command_buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eTransfer,
				{},
				{},
				{},
				barrier
			);

			vk::ArrayWrapper1D<vk::Offset3D, 2> offsets;
			vk::ArrayWrapper1D<vk::Offset3D, 2> dst_offsets;
			offsets[0] = vk::Offset3D(0, 0, 0);
			offsets[1] = vk::Offset3D(mip_width, mip_height, 1);
			dst_offsets[0] = vk::Offset3D(0, 0, 0);
			dst_offsets[1] = vk::Offset3D(
				mip_width > 1 ? mip_width / 2 : 1,
				mip_height > 1 ? mip_height / 2 : 1,
				1
			);

			vk::ImageBlit blit = {
				.srcSubresource =
					{
						.aspectMask = vk::ImageAspectFlagBits::eColor,
						.mipLevel = i - 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				.srcOffsets = offsets,
				.dstSubresource =
					{
						.aspectMask = vk::ImageAspectFlagBits::eColor,
						.mipLevel = i,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				.dstOffsets = dst_offsets,
			};

			command_buffer.blitImage(
				image,
				vk::ImageLayout::eTransferSrcOptimal,
				image,
				vk::ImageLayout::eTransferDstOptimal,
				blit,
				vk::Filter::eLinear
			);

			barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
			barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			command_buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eFragmentShader,
				{},
				{},
				{},
				barrier
			);

			if (mip_width > 1) {
				mip_width /= 2;
			}
			if (mip_height > 1) {
				mip_height /= 2;
			}
		}
		barrier.subresourceRange.baseMipLevel = mip_levels - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader,
			{},
			{},
			{},
			barrier
		);

		end_single_time_commands(command_buffer);
	}

	void create_texture_image_view() {
		texture_image_view = create_image_view(
			texture_image,
			vk::Format::eR8G8B8A8Srgb,
			vk::ImageAspectFlagBits::eColor,
			mip_levels
		);
	}

	void create_texture_sampler() {
		vk::PhysicalDeviceProperties properties =
			physical_device.get_physical_device().getProperties();
		vk::SamplerCreateInfo sampler_info {
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eLinear,
			.addressModeU = vk::SamplerAddressMode::eRepeat,
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.addressModeW = vk::SamplerAddressMode::eRepeat,
			.mipLodBias = 0.0f,
			.anisotropyEnable = vk::True,
			.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
			.compareEnable = vk::False,
			.compareOp = vk::CompareOp::eAlways,
		};
		texture_sampler = vk::raii::Sampler(device.get_device(), sampler_info);
	}

	void create_image(
		uint32_t width,
		uint32_t height,
		uint32_t mip_levels,
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
			.mipLevels = mip_levels,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = tiling,
			.usage = usage,
			.sharingMode = vk::SharingMode::eExclusive,
		};

		image = vk::raii::Image(device.get_device(), image_info);

		vk::MemoryRequirements memory_requirements = image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocate_info {
			.allocationSize = memory_requirements.size,
			.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties),
		};

		image_memory = vk::raii::DeviceMemory(device.get_device(), allocate_info);
		image.bindMemory(image_memory, 0);
	}

	vk::raii::CommandBuffer begin_single_time_commands() {
		vk::raii::CommandBuffer command_buffer = device.allocate_command_buffer();

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

		device.submit_graphics(submit_info);
		device.wait_graphics_queue();
	}

	void create_command_buffers() {
		command_buffers = device.allocate_command_buffer(MAX_FRAMES_IN_FLIGHT);
	}

	void load_model() {
		model.load_from_obj_file(MODEL_PATH.c_str());

		debug_log("Loading model done!");
		debug_log("  Vertices: {}", model.get_vertices().size());
		debug_log("  Indices: {}", model.get_indices().size());
	}

	void create_vertex_buffer() {
		auto buffer_size = sizeof(model.get_vertices()[0]) * model.get_vertices().size();

		tramogi::graphics::StagingBuffer staging_buffer;
		auto result = staging_buffer.init(device, buffer_size);
		if (!result) {
			throw std::runtime_error(result.error());
		}
		staging_buffer.map();
		staging_buffer.upload_data(model.get_vertices().data());
		staging_buffer.unmap();

		result = vertex_buffer.init(device, buffer_size);
		if (!result) {
			throw std::runtime_error(result.error());
		}

		copy_buffer(staging_buffer.get_buffer(), vertex_buffer.get_buffer(), buffer_size);
	}

	void create_index_buffer() {
		auto buffer_size = sizeof(model.get_indices()[0]) * model.get_indices().size();

		tramogi::graphics::StagingBuffer staging_buffer;
		auto result = staging_buffer.init(device, buffer_size);
		if (!result) {
			throw std::runtime_error(result.error());
		}
		staging_buffer.map();
		staging_buffer.upload_data(model.get_indices().data());
		staging_buffer.unmap();

		result = index_buffer.init(device, buffer_size);
		if (!result) {
			throw std::runtime_error(result.error());
		}
		copy_buffer(staging_buffer.get_buffer(), index_buffer.get_buffer(), buffer_size);
	}

	void create_uniform_buffers() {
		uniform_buffers.clear();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vk::DeviceSize buffer_size = sizeof(UniformBufferObject);
			tramogi::graphics::UniformBuffer ubo;
			auto result = ubo.init(device, buffer_size);
			if (!result) {
				throw std::runtime_error(result.error());
			}
			ubo.map();

			uniform_buffers.emplace_back(std::move(ubo));
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

		descriptor_pool = vk::raii::DescriptorPool(device.get_device(), pool_info);
	}

	void create_descriptor_sets() {
		descriptor_sets.clear();

		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptor_set_layout);
		vk::DescriptorSetAllocateInfo allocate_info {
			.descriptorPool = descriptor_pool,
			.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
			.pSetLayouts = layouts.data(),
		};

		descriptor_sets = device.get_device().allocateDescriptorSets(allocate_info);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vk::DescriptorBufferInfo buffer_info {
				.buffer = uniform_buffers[i].get_buffer(),
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
			device.get_device().updateDescriptorSets(descriptor_writes, {});
		}
	}

	void copy_buffer(vk::raii::Buffer &src, vk::raii::Buffer &dst, vk::DeviceSize size) {
		vk::raii::CommandBuffer command_copy_buffer = begin_single_time_commands();
		command_copy_buffer.copyBuffer(
			src,
			dst,
			vk::BufferCopy {
				.srcOffset = 0,
				.dstOffset = 0,
				.size = size,
			}
		);
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
		auto memory_properties = physical_device.get_memory_properties();
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
		vk::ImageLayout new_layout,
		uint32_t mip_levels
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
				.levelCount = mip_levels,
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

		command_buffers[current_frame].bindVertexBuffers(0, *vertex_buffer.get_buffer(), {0});
		command_buffers[current_frame]
			.bindIndexBuffer(*index_buffer.get_buffer(), 0, vk::IndexType::eUint32);

		command_buffers[current_frame].bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pipeline_layout,
			0,
			*descriptor_sets[current_frame],
			nullptr
		);

		// command_buffers[current_frame].draw(3, 1, 1, 0);
		command_buffers[current_frame].drawIndexed(model.get_indices().size(), 1, 0, 0, 0);
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

	void draw_frame(double delta) {
		device.wait_idle(current_frame);

		try {
			auto [result, image_index] = swapchain.acquireNextImage(
				UINT64_MAX,
				*device.get_present_semaphore(current_frame),
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
			device.reset_fence(current_frame);

			update_uniform_buffer(current_frame, delta);

			vk::PipelineStageFlags wait_destination_stage_mask(
				vk::PipelineStageFlagBits::eColorAttachmentOutput
			);
			const vk::SubmitInfo submit_info {
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &*device.get_present_semaphore(current_frame),
				.pWaitDstStageMask = &wait_destination_stage_mask,
				.commandBufferCount = 1,
				.pCommandBuffers = &*command_buffers[current_frame],
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &*device.get_render_semaphore(current_frame),
			};

			device.submit_graphics(submit_info, current_frame, true);

			vk::PresentInfoKHR present_info {
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &*device.get_render_semaphore(current_frame),
				.swapchainCount = 1,
				.pSwapchains = &*swapchain,
				.pImageIndices = &image_index,
			};

			auto present_result = device.present(present_info);
			if (!present_result || window.resized) {
				window.resized = false;
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

	void update_uniform_buffer(uint32_t current_image, double delta) {
		static glm::mat4 pos(1.0f);
		static auto start_time = std::chrono::high_resolution_clock::now();

		auto current_time = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(
						 current_time - start_time
		)
						 .count();

		constexpr float speed = 3.0f;

		if (input.is_pressed(tramogi::input::Key::W)) {
			pos = glm::translate(pos, glm::vec3(0.0f, -speed * delta, 0.0f));
		}
		if (input.is_pressed(tramogi::input::Key::A)) {
			pos = glm::translate(pos, glm::vec3(speed * delta, 0.0f, 0.0f));
		}
		if (input.is_pressed(tramogi::input::Key::S)) {
			pos = glm::translate(pos, glm::vec3(0.0f, speed * delta, 0.0f));
		}
		if (input.is_pressed(tramogi::input::Key::D)) {
			pos = glm::translate(pos, glm::vec3(-speed * delta, 0.0f, 0.0f));
		}

		UniformBufferObject ubo;
		ubo.model = glm::scale(
			glm::rotate(pos, time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			glm::vec3(2.0f)
		);
		ubo.view = glm::lookAt(
			glm::vec3(0.0f, 5.0f, 1.0f),
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

		uniform_buffers[current_image].upload_data(&ubo);
	}

	void cleanup_swapchain() {
		swapchain_images.clear();
		swapchain = nullptr;
	}

	void recreate_swapchain() {
		Dimension dimension = window.get_size();
		while (dimension.width == 0 || dimension.height == 0) {
			dimension = window.get_size();
			window.wait_events();
		}

		device.wait_idle(current_frame);

		cleanup_swapchain();

		create_swapchain();
		create_image_views();
		create_depth_resources();

		debug_log("Swapchain resized to {}x{}", dimension.width, dimension.height);
	}
};

int main() {
	debug_log("Running in DEBUG mode");

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

	debug_log("Exited successfully");
	return EXIT_SUCCESS;
}
