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
#include <imgui_internal.h>
#include <limits>
#include <memory>
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
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/trigonometric.hpp>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include "engine/camera.h"
#include "engine/primitives/cube.h"
#include "graphics/allocator.h"
#include "graphics/command_buffer.h"
#include "graphics/device.h"
#include "graphics/dispatch_loader.h"
#include "graphics/image.h"
#include "graphics/instance.h"
#include "graphics/physical_device.h"
#include "graphics/surface.h"
#include "graphics/swapchain.h"
#include "tramogi/core/io/file.h"
#include "tramogi/core/io/image_data.h"
#include "tramogi/core/logging/logging.h"
#include "tramogi/core/types.h"
#include "tramogi/graphics/buffer.h"
#include "tramogi/graphics/imgui/imgui_loader.h"
#include "tramogi/input/keyboard.h"
#include "tramogi/input/mouse.h"
#include "tramogi/platform/window.h"

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;
const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

using namespace tramogi::core;
using namespace tramogi::engine::primitives;
using namespace tramogi::graphics;
using namespace tramogi::input;
using namespace tramogi::platform;

using namespace tramogi::core::logging;

struct UniformBufferObject {
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 model;
};

class ProjectSkyHigh {
public:
	ProjectSkyHigh()
		: window(WIDTH, HEIGHT, "Tramogi Demo"), instance(window.get_required_extensions()),
		  physical_device(instance, window.create_surface(instance.get_instance())),
		  device(physical_device, instance), swpchain(physical_device, device, window.get_size()),
		  camera(1280, 720, glm::radians(90.0f)), model(1.0f) {
		camera.set_position({0, 0, -8});
	}

	void run() {
		init_window();
		init_vulkan();
		init_imgui();
		main_loop();
		cleanup();
	}

private:
	Window window;

	Instance instance;
	PhysicalDevice physical_device;
	Device device;

	Swapchain swpchain;

	vk::raii::DescriptorSetLayout descriptor_set_layout = nullptr;
	vk::raii::PipelineLayout pipeline_layout = nullptr;
	vk::raii::Pipeline graphics_pipeline = nullptr;

	std::vector<CommandBuffer> command_buffers;

	std::unique_ptr<VertexBuffer> vertex_buffer;
	std::unique_ptr<IndexBuffer> index_buffer;
	std::vector<UniformBuffer> uniform_buffers;

	vk::raii::DescriptorPool descriptor_pool = nullptr;
	std::vector<vk::raii::DescriptorSet> descriptor_sets;

	uint32_t mip_levels = 0;
	vk::raii::Image texture_image = nullptr;
	vk::raii::DeviceMemory texture_memory = nullptr;
	vk::raii::ImageView texture_image_view = nullptr;
	vk::raii::Sampler texture_sampler = nullptr;

	std::unique_ptr<ImageViewPair<DepthImage>> depth_image;

	uint32_t current_frame = 0;

	Camera camera;

	Keyboard key_input;
	Mouse mouse_input;

	Cube model;

	bool is_imgui_visible = false;

	void init_window() {
		window.set_key_callback([this](int scancode, bool is_pressed) {
			key_input.set_key(scancode, is_pressed);
		});
		window.set_mouse_callback(
			[this](int button, bool is_pressed) {
				mouse_input.set_mouse_button(button, is_pressed);
			},
			[this](double x, double y) { mouse_input.set_mouse_position(x, y); }
		);
	}

	void init_vulkan() {
		create_descriptor_layout();
		create_graphics_pipeline();
		create_depth_resources();
		create_texture_image();
		create_texture_image_view();
		create_texture_sampler();
		create_vertex_buffer();
		create_index_buffer();
		create_uniform_buffers();
		create_descriptor_pool();
		create_descriptor_sets();
		create_command_buffers();
	}

	void init_imgui() {
		debug_log("Starting ImGui setup");

		vk::PipelineRenderingCreateInfoKHR dynamic_render_info {};
		dynamic_render_info.colorAttachmentCount = 1;
		dynamic_render_info.pColorAttachmentFormats = &swpchain.get_format();
		dynamic_render_info.depthAttachmentFormat = physical_device.get_depth_format().value();

		ImGui_ImplVulkan_InitInfo imgui_info {};
		imgui_info.Instance = *instance.get_instance();
		imgui_info.PhysicalDevice = *physical_device.get_physical_device();
		imgui_info.Device = *device.get_device();
		imgui_info.QueueFamily = physical_device.get_graphics_queue_index();
		imgui_info.Queue = *device.get_graphics_queue();
		imgui_info.DescriptorPool = *descriptor_pool;
		imgui_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
		imgui_info.ImageCount = MAX_FRAMES_IN_FLIGHT;
		imgui_info.UseDynamicRendering = true;
		imgui_info.PipelineInfoMain.Subpass = 0;
		imgui_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		imgui_info.PipelineInfoMain.PipelineRenderingCreateInfo = dynamic_render_info;

		tramogi::graphics::imgui::init(window, &imgui_info);
	}

	void main_loop() {
		auto last_time = std::chrono::high_resolution_clock().now();
		uint32_t frames = 0;
		double timer = 0;
		bool print_fps = false;

		glm::vec2 last_pos;
		bool drag_first_frame = true;

		while (!window.should_close()) {
			auto now = std::chrono::high_resolution_clock().now();
			double delta =
				std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_time).count() /
				1000000000.0;

			window.poll_events();

			tramogi::graphics::imgui::next_frame();

			if (key_input.is_pressed(Key::F3)) {
				print_fps = !print_fps;
				debug_log("Print FPS: {}", print_fps);
				key_input.consume_key(tramogi::input::Key::F3);
			} else if (key_input.is_pressed(Key::F4)) {
				is_imgui_visible = !is_imgui_visible;
				key_input.consume_key(Key::F4);
			}
			if (key_input.is_pressed(Key::Q)) {
				window.request_close();
			}

			bool drag_rotating = mouse_input.is_pressed(MouseButton::Middle) ||
								 (key_input.is_pressed(Key::Control) &&
								  mouse_input.is_pressed(MouseButton::Right));
			if (drag_rotating) {
				if (!drag_first_frame) {
					camera.rotate_to_poi(
						glm::radians(1.0f * (mouse_input.get_y() - last_pos.y)),
						glm::radians(1.0f * (mouse_input.get_x() - last_pos.x))
					);
				}
				last_pos = glm::vec2(mouse_input.get_x(), mouse_input.get_y());
			}

			drag_first_frame = !drag_rotating;

			camera.update_view();
			if (is_imgui_visible) {
				ImGui::Begin("Misc");
				ImGui::Text(
					"Position %.3f %.3f %.3f",
					camera.get_position().x,
					camera.get_position().y,
					camera.get_position().z
				);
				ImGui::Text("Mouse %.3f %.3f", mouse_input.get_x(), mouse_input.get_y());
				ImGui::End();
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
		tramogi::graphics::imgui::cleanup();
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

		Size dimension = window.get_size();

		return {
			std::clamp<uint32_t>(
				dimension.x,
				capabilities.minImageExtent.width,
				capabilities.maxImageExtent.width
			),
			std::clamp<uint32_t>(
				dimension.y,
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

		constexpr vk::VertexInputBindingDescription binding_description =
			{0, sizeof(BasicVertex), vk::VertexInputRate::eVertex};
		constexpr std::array<vk::VertexInputAttributeDescription, 2> attribute_descriptions = {
			vk::VertexInputAttributeDescription {
				0,
				0,
				vk::Format::eR32G32B32Sfloat,
				offsetof(BasicVertex, position)
			},
			vk::VertexInputAttributeDescription {
				1,
				0,
				vk::Format::eR32G32B32Sfloat,
				offsetof(BasicVertex, color)
			}
		};
		vk::PipelineVertexInputStateCreateInfo vertex_input_info {
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &binding_description,
			.vertexAttributeDescriptionCount = attribute_descriptions.size(),
			.pVertexAttributeDescriptions = attribute_descriptions.data(),
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
			.pColorAttachmentFormats = &swpchain.get_format(),
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
		depth_image = std::make_unique<ImageViewPair<DepthImage>>(
			physical_device,
			device,
			swpchain.get_extent().width,
			swpchain.get_extent().height,
			false
		);
	}

	void create_texture_image() {
		ImageData image_data;
		if (!image_data.load_from_file(TEXTURE_PATH.c_str())) {
			// TODO: handle missing texture without throwing
			throw std::runtime_error("Failed to load texture image");
		}

		int texture_width = image_data.get_width();
		int texture_height = image_data.get_height();
		mip_levels = image_data.get_mip_levels();
		vk::DeviceSize image_size = image_data.get_size();

		StagingBuffer staging_buffer(device, image_size);

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
		CommandBuffer cmd = device.allocate_command_buffer();
		cmd.begin_onetime();

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

			cmd.get_command_buffer().pipelineBarrier(
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

			cmd.get_command_buffer().blitImage(
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

			cmd.get_command_buffer().pipelineBarrier(
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

		cmd.get_command_buffer().pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader,
			{},
			{},
			{},
			barrier
		);

		cmd.end();
		device.submit(cmd);
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

	void create_command_buffers() {
		command_buffers = device.allocate_command_buffers(MAX_FRAMES_IN_FLIGHT);
	}

	void create_vertex_buffer() {
		auto buffer_size = sizeof(model.get_vertices()[0]) * model.get_vertices().size();

		StagingBuffer staging_buffer(device, buffer_size);
		staging_buffer.map();
		staging_buffer.upload_data(model.get_vertices().data());
		staging_buffer.unmap();

		vertex_buffer = std::make_unique<VertexBuffer>(device, buffer_size);
		copy_buffer(staging_buffer.get_buffer(), vertex_buffer->get_buffer(), buffer_size);
	}

	void create_index_buffer() {
		auto buffer_size = sizeof(model.get_indices()[0]) * model.get_indices().size();

		StagingBuffer staging_buffer(device, buffer_size);
		staging_buffer.map();
		staging_buffer.upload_data(model.get_indices().data());
		staging_buffer.unmap();

		index_buffer = std::make_unique<IndexBuffer>(device, buffer_size);
		copy_buffer(staging_buffer.get_buffer(), index_buffer->get_buffer(), buffer_size);
	}

	void create_uniform_buffers() {
		uniform_buffers.clear();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vk::DeviceSize buffer_size = sizeof(UniformBufferObject);
			UniformBuffer ubo(device, buffer_size);
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
				.descriptorCount = MAX_FRAMES_IN_FLIGHT + 1,
			},
		};

		vk::DescriptorPoolCreateInfo pool_info {
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = MAX_FRAMES_IN_FLIGHT + 1,
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
		CommandBuffer cmd = device.allocate_command_buffer();
		cmd.begin_onetime();
		cmd.get_command_buffer().copyBuffer(
			src,
			dst,
			vk::BufferCopy {
				.srcOffset = 0,
				.dstOffset = 0,
				.size = size,
			}
		);
		cmd.end();
		device.submit(cmd);
	}

	void copy_buffer_to_image(
		const vk::raii::Buffer &buffer,
		vk::raii::Image &image,
		uint32_t width,
		uint32_t height
	) {
		CommandBuffer cmd = device.allocate_command_buffer();
		cmd.begin_onetime();

		vk::BufferImageCopy region {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
			.imageOffset = {0, 0, 0},
			.imageExtent = {width, height, 1},
		};

		cmd.get_command_buffer()
			.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

		cmd.end();
		device.submit(cmd);
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
		CommandBuffer cmd = device.allocate_command_buffer();
		cmd.begin_onetime();

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

		cmd.get_command_buffer()
			.pipelineBarrier(source_stage, destination_stage, {}, {}, nullptr, barrier);

		cmd.end();
		device.submit(cmd);
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

		command_buffers[current_frame].get_command_buffer().pipelineBarrier2(dependency_info);
	}

	void record_command_buffer(uint32_t image_index) {
		CommandBuffer &command_buffer = command_buffers[current_frame];
		command_buffer.begin();

		transition_image_layout(
			swpchain.get_image(image_index),
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::ImageAspectFlagBits::eColor
		);
		depth_image->get_image().as_depth_target(command_buffers[current_frame]);

		vk::ClearValue clear_color = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue clear_depth = vk::ClearDepthStencilValue(1.0f, 0);
		vk::RenderingAttachmentInfo attachment_info {
			.imageView = swpchain.get_image_view(image_index).get_image_view(),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clear_color,
		};
		vk::RenderingAttachmentInfo depth_attachment_info {
			.imageView = depth_image->get_image_view().get_image_view(),
			.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = clear_depth,
		};

		vk::RenderingInfo rendering_info {
			.renderArea =
				{
					.offset = {0, 0},
					.extent = swpchain.get_extent(),
				},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &attachment_info,
			.pDepthAttachment = &depth_attachment_info,
		};

		command_buffer.get_command_buffer().beginRendering(rendering_info);

		command_buffer.get_command_buffer().bindPipeline(
			vk::PipelineBindPoint::eGraphics,
			graphics_pipeline
		);
		command_buffer.get_command_buffer().setViewport(
			0,
			vk::Viewport(
				0.0f,
				0.0f,
				swpchain.get_extent().width,
				swpchain.get_extent().height,
				0.0f,
				1.0f
			)
		);
		command_buffer.get_command_buffer().setScissor(
			0,
			vk::Rect2D(vk::Offset2D(0, 0), swpchain.get_extent())
		);

		command_buffer.get_command_buffer().bindVertexBuffers(0, *vertex_buffer->get_buffer(), {0});
		command_buffer.get_command_buffer()
			.bindIndexBuffer(*index_buffer->get_buffer(), 0, vk::IndexType::eUint32);

		command_buffer.get_command_buffer().bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pipeline_layout,
			0,
			*descriptor_sets[current_frame],
			nullptr
		);

		command_buffer.get_command_buffer()
			.drawIndexed(model.get_indices().size(), 11 * 11 * 11, 0, 0, 0);

		tramogi::graphics::imgui::render(command_buffer.get_command_buffer());

		command_buffer.get_command_buffer().endRendering();

		transition_image_layout(
			swpchain.get_image(image_index),
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			{},
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eBottomOfPipe,
			vk::ImageAspectFlagBits::eColor
		);

		command_buffer.end();
	}

	void draw_frame(double delta) {
		device.wait_idle(current_frame);
		auto image_index = swpchain.get_next_image(current_frame);

		if (!image_index) {
			recreate_swapchain();
			return;
		}

		command_buffers[current_frame].get_command_buffer().reset();

		update_uniform_buffer(current_frame, delta);
		record_command_buffer(image_index.value());
		device.reset_fence(current_frame);

		vk::PipelineStageFlags wait_destination_stage_mask(
			vk::PipelineStageFlagBits::eColorAttachmentOutput
		);
		const vk::SubmitInfo submit_info {
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*device.get_present_semaphore(current_frame),
			.pWaitDstStageMask = &wait_destination_stage_mask,
			.commandBufferCount = 1,
			.pCommandBuffers = &*command_buffers[current_frame].get_command_buffer(),
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &*device.get_render_semaphore(current_frame),
		};

		device.submit_graphics(submit_info, current_frame, true);

		vk::PresentInfoKHR present_info {
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*device.get_render_semaphore(current_frame),
			.swapchainCount = 1,
			.pSwapchains = &*swpchain.get_swapchain(),
			.pImageIndices = &image_index.value(),
		};

		auto present_result = device.present(present_info);
		if (!present_result || window.resized) {
			window.resized = false;
			recreate_swapchain();
		}

		current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void update_uniform_buffer(uint32_t current_image, [[maybe_unused]] double delta) {
		static glm::vec3 rot;
		static glm::vec3 pos_translate;
		if (is_imgui_visible) {
			ImGui::Begin("Properties");
			ImGui::DragFloat3("Position", &pos_translate.x, 0.1f, 0, 0, "%0.1f");
			ImGui::DragFloat3("Rotation", &rot.x, 0.1f, 0, 360, "%.1f");
			ImGui::End();
		}

		UniformBufferObject ubo;
		ubo.model = glm::rotate(
			glm::rotate(
				glm::rotate(
					glm::translate(glm::mat4(1.0f), pos_translate),
					glm::radians(rot.z),
					glm::vec3(0.0f, 0.0f, 1.0f)
				),
				glm::radians(rot.y),
				glm::vec3(0.0f, 1.0f, 0.0f)
			),
			glm::radians(rot.x),
			glm::vec3(1.0f, 0.0f, 0.0f)
		);
		ubo.view = camera.get_view();
		ubo.projection = camera.get_projection();

		uniform_buffers[current_image].upload_data(&ubo);
	}

	void recreate_swapchain() {
		Size dimension = window.get_size();
		while (dimension.x == 0 || dimension.y == 0) {
			dimension = window.get_size();
			window.wait_events();
		}

		device.wait_idle(current_frame);

		swpchain.recreate(dimension);

		create_depth_resources();

		camera.change_perspective(dimension.x, dimension.y, glm::radians(90.0f));

		debug_log("Swapchain resized to {}x{}", dimension.x, dimension.y);
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
