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
#include <glm/matrix.hpp>
#include <initializer_list>
#include <memory>
#include <print>
#include <stdexcept>
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

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

#include "engine/camera.h"
#include "engine/primitives/cube.h"
#include "graphics/allocator.h"
#include "graphics/attachment_info.h"
#include "graphics/command_buffer.h"
#include "graphics/descriptor.h"
#include "graphics/descriptor_pool.h"
#include "graphics/device.h"
#include "graphics/dispatch_loader.h"
#include "graphics/format.h"
#include "graphics/image.h"
#include "graphics/image_view.h"
#include "graphics/instance.h"
#include "graphics/internal_types.h"
#include "graphics/physical_device.h"
#include "graphics/pipeline.h"
#include "graphics/shader.h"
#include "graphics/surface.h"
#include "graphics/swapchain.h"
#include "graphics/vertex_descriptor.h"
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
constexpr float FOV = 90;

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

using namespace tramogi::core;
using namespace tramogi::engine::primitives;
using namespace tramogi::graphics;
using namespace tramogi::input;
using namespace tramogi::platform;

using namespace tramogi::core::logging;

struct UniformBufferObject {
	glm::mat4 projection_view;
	glm::mat4 inverse_projection_view;

	alignas(16) glm::vec3 camera_position;
	alignas(16) glm::vec3 camera_forward;
	alignas(16) glm::vec3 camera_right;
	alignas(16) glm::vec3 camera_up;
	float z_near;
	float z_far;

	alignas(16) glm::vec3 world_light_direction;

	alignas(16) glm::mat4 model;
	glm::mat4 model_normal;

	int32_t gbuffer_debug;
};

class ProjectSkyHigh {
public:
	ProjectSkyHigh()
		: window(WIDTH, HEIGHT, "Tramogi Demo"), instance(window.get_required_extensions()),
		  physical_device(instance, window.create_surface(instance.get_instance())),
		  device(physical_device, instance), swapchain(physical_device, device, window.get_size()),
		  descriptor_pool(device, MAX_FRAMES_IN_FLIGHT + 1),
		  camera_descriptor_layout(device, camera_binds),
		  shading_descriptor_layout(device, shading_binds),
		  skybox_descriptor_layout(device, skybox_binds),
		  sky_cubemap(physical_device, device, 512, 512, Format::RGBA8Srgb, false),
		  camera(WIDTH, HEIGHT, glm::radians(FOV)), model(1.0f) {
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

	Swapchain swapchain;

	std::vector<CommandBuffer> command_buffers;

	UniquePtr<VertexBuffer> vertex_buffer;
	UniquePtr<IndexBuffer> index_buffer;
	std::vector<UniformBuffer> uniform_buffers;

	DescriptorPool descriptor_pool;
	DescriptorLayout camera_descriptor_layout;
	DescriptorLayout shading_descriptor_layout;
	DescriptorLayout skybox_descriptor_layout;
	std::vector<DescriptorSet> camera_descriptor_sets;
	std::vector<DescriptorSet> shading_descriptor_sets;
	std::vector<DescriptorSet> skybox_descriptor_sets;

	UniquePtr<Pipeline> gbuffer_pipeline;
	UniquePtr<Pipeline> shading_pipeline;
	UniquePtr<Pipeline> skybox_pipeline;

	constexpr static std::array camera_binds = {
		DescriptorLayoutBinding {
			.location = 0,
			.stage = DescriptorLayoutBinding::Stage::VertexFragment,
			.type = DescriptorLayoutBinding::Type::UniformbBuffer,
		},
	};

	constexpr static std::array shading_binds = {
		DescriptorLayoutBinding {
			.location = 0,
			.count = 3,
			.stage = DescriptorLayoutBinding::Stage::Fragment,
			.type = DescriptorLayoutBinding::Type::CombinedSampler,
		},
	};

	constexpr static std::array skybox_binds = {
		DescriptorLayoutBinding {
			.location = 0,
			.count = 1,
			.stage = DescriptorLayoutBinding::Stage::Fragment,
			.type = DescriptorLayoutBinding::Type::CombinedSampler,
		},
		DescriptorLayoutBinding {
			.location = 1,
			.count = 1,
			.stage = DescriptorLayoutBinding::Stage::Fragment,
			.type = DescriptorLayoutBinding::Type::CombinedSampler,
		},
	};

	UniquePtr<InFlightSet<DepthImage>> depth_image;
	UniquePtr<InFlightSet<Image>> gbuffer_albedo;
	UniquePtr<InFlightSet<Image>> gbuffer_normal;
	UniquePtr<InFlightSet<Image>> offscreen_framebuffer;

	AttachmentLayout gbuffer_attachment_layout;
	AttachmentLayout shading_attachment_layout;
	AttachmentLayout skybox_attachment_layout;

	vk::raii::Sampler sampler = nullptr;

	ImageViewPair<CubeMapImage> sky_cubemap;

	uint32_t current_frame = 0;

	Camera camera;

	Keyboard key_input;
	Mouse mouse_input;

	Cube model;

	bool is_imgui_visible = false;

	glm::vec3 world_light_direction = glm::vec3(0.3, -0.6, 0.5);

	int32_t gbuffer_debug = 0;
	bool show_skybox = true;

	void init_window() {
		window.set_key_callback([this](int scancode, bool is_pressed) {
			key_input.set_key(scancode, is_pressed);
		});
		window.set_mouse_callback(
			[this](int button, bool is_pressed) {
				mouse_input.set_mouse_button(button, is_pressed);
			},
			[this](double x, double y) { mouse_input.set_mouse_position(x, y); },
			[this](double, double y_offset) {
				camera.set_orbit_distance(camera.get_orbit_distance() + y_offset);
			}
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
		sampler = vk::raii::Sampler(device.get_device(), sampler_info);
	}
	void init_vulkan() {
		create_image_resources();
		create_graphics_pipeline();
		create_shading_pipeline();
		create_skybox_pipeline();

		create_texture_sampler();
		create_vertex_buffer();
		create_index_buffer();
		create_uniform_buffers();
		camera_descriptor_sets = device.allocate_descriptor_sets(
			descriptor_pool,
			camera_descriptor_layout,
			MAX_FRAMES_IN_FLIGHT
		);
		shading_descriptor_sets = device.allocate_descriptor_sets(
			descriptor_pool,
			shading_descriptor_layout,
			MAX_FRAMES_IN_FLIGHT
		);
		skybox_descriptor_sets = device.allocate_descriptor_sets(
			descriptor_pool,
			skybox_descriptor_layout,
			MAX_FRAMES_IN_FLIGHT
		);
		create_descriptor_sets();
		create_command_buffers();
		create_cubemap();
	}

	void init_imgui() {
		debug_log("Starting ImGui setup");

		vk::Format format = native(swapchain.get_format());

		vk::PipelineRenderingCreateInfoKHR dynamic_render_info {};
		dynamic_render_info.colorAttachmentCount = 1;
		dynamic_render_info.pColorAttachmentFormats = &format;
		dynamic_render_info.depthAttachmentFormat = native(
			physical_device.get_depth_format().value()
		);

		ImGui_ImplVulkan_InitInfo imgui_info {};
		imgui_info.Instance = *instance.get_instance();
		imgui_info.PhysicalDevice = *physical_device.get_physical_device();
		imgui_info.Device = *device.get_device();
		imgui_info.QueueFamily = physical_device.get_graphics_queue_index();
		imgui_info.Queue = *device.get_graphics_queue();
		imgui_info.DescriptorPool = *descriptor_pool.get_descriptor_pool();
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

				ImGui::RadioButton("Shaded", &gbuffer_debug, 0);
				ImGui::SameLine();
				ImGui::RadioButton("Albedo", &gbuffer_debug, 1);
				ImGui::RadioButton("Normal", &gbuffer_debug, 2);
				ImGui::SameLine();
				ImGui::RadioButton("Depth", &gbuffer_debug, 3);
				ImGui::RadioButton("Position", &gbuffer_debug, 4);

				ImGui::Checkbox("Skybox", &show_skybox);

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

	void create_graphics_pipeline() {
		auto shader_code_result = read_shader_file("shaders/slang.spv");
		if (!shader_code_result) {
			throw std::runtime_error(shader_code_result.error());
		}
		auto shader_code = shader_code_result.value();

		Shader shader_module(device, shader_code);
		shader_module.add_vertex_stage("vert_main");
		shader_module.add_fragment_stage("frag_main");

		VertexDescriptor vertex_descriptor(VertexDescriptor::Type::Vertex, 0, sizeof(BasicVertex));
		vertex_descriptor.add_attributes({
			.location = 0,
			.format = Format::Float3,
			.offset = offsetof(BasicVertex, position),
		});
		vertex_descriptor.add_attributes({
			.location = 1,
			.format = Format::Float3,
			.offset = offsetof(BasicVertex, color),
		});
		vertex_descriptor.add_attributes({
			.location = 2,
			.format = Format::Float3,
			.offset = offsetof(BasicVertex, normal),
		});

		gbuffer_attachment_layout.add_attachment(
			AttachmentLayout::Type::Depth,
			depth_image->get_image(0).get_format()
		);
		gbuffer_attachment_layout.add_attachment(
			AttachmentLayout::Type::Color0,
			gbuffer_albedo->get_image(0).get_format()
		);
		gbuffer_attachment_layout.add_attachment(
			AttachmentLayout::Type::Color1,
			gbuffer_normal->get_image(0).get_format()
		);

		gbuffer_pipeline = std::make_unique<Pipeline>(
			device,
			shader_module,
			std::initializer_list {
				&camera_descriptor_layout,
			},
			vertex_descriptor,
			gbuffer_attachment_layout,
			PipelineOption {
				.is_depth_test = true,
				.is_depth_write = true,
			}
		);
	}

	void create_shading_pipeline() {
		auto shader_code_result = read_shader_file("shaders/slang.spv");
		if (!shader_code_result) {
			throw std::runtime_error(shader_code_result.error());
		}
		auto shader_code = shader_code_result.value();

		Shader shader_module(device, shader_code);
		shader_module.add_vertex_stage("screen_vert");
		shader_module.add_fragment_stage("screen_frag");

		VertexDescriptor vertex_descriptor(VertexDescriptor::Type::Vertex, 0, 0);

		shading_attachment_layout.add_attachment(
			AttachmentLayout::Type::Color0,
			offscreen_framebuffer->get_image(0).get_format()
		);

		shading_pipeline = std::make_unique<Pipeline>(
			device,
			shader_module,
			std::initializer_list {
				&camera_descriptor_layout,
				&shading_descriptor_layout,
			},
			vertex_descriptor,
			shading_attachment_layout,
			PipelineOption {
				.is_depth_test = false,
				.is_depth_write = false,
			}
		);
	}

	void create_skybox_pipeline() {
		auto shader_code_result = read_shader_file("shaders/slang.spv");
		if (!shader_code_result) {
			throw std::runtime_error(shader_code_result.error());
		}
		auto shader_code = shader_code_result.value();

		Shader shader_module(device, shader_code);
		shader_module.add_vertex_stage("screen_vert");
		shader_module.add_fragment_stage("skybox_frag");

		skybox_attachment_layout.add_attachment(
			AttachmentLayout::Type::Color0,
			swapchain.get_format()
		);
		skybox_attachment_layout.add_attachment(
			AttachmentLayout::Type::Depth,
			depth_image->get_image(0).get_format()
		);

		skybox_attachment_layout.set_load_operation(
			AttachmentLayout::Type::Color0,
			AttachmentLayout::LoadOperation::Load
		);
		skybox_attachment_layout.set_load_operation(
			AttachmentLayout::Type::Depth,
			AttachmentLayout::LoadOperation::Load
		);

		VertexDescriptor vertex_descriptor(VertexDescriptor::Type::Vertex, 0, 0);
		skybox_pipeline = std::make_unique<Pipeline>(
			device,
			shader_module,
			std::initializer_list {
				&camera_descriptor_layout,
				&skybox_descriptor_layout,
			},
			vertex_descriptor,
			skybox_attachment_layout,
			PipelineOption {
				.is_depth_test = true,
				.is_depth_write = false,
			}
		);
	}

	void create_image_resources() {
		depth_image = std::make_unique<InFlightSet<DepthImage>>(
			physical_device,
			device,
			swapchain.get_extent().width,
			swapchain.get_extent().height,
			false
		);
		gbuffer_albedo = std::make_unique<InFlightSet<Image>>(
			physical_device,
			device,
			swapchain.get_extent().width,
			swapchain.get_extent().height,
			Format::RGBA8Srgb,
			Image::Usage::SampledColorTarget,
			false
		);
		gbuffer_normal = std::make_unique<InFlightSet<Image>>(
			physical_device,
			device,
			swapchain.get_extent().width,
			swapchain.get_extent().height,
			Format::RGBA16Float,
			Image::Usage::SampledColorTarget,
			false
		);
		offscreen_framebuffer = std::make_unique<InFlightSet<Image>>(
			physical_device,
			device,
			swapchain.get_extent().width,
			swapchain.get_extent().height,
			Format::RGBA8Srgb,
			Image::Usage::SampledColorTarget,
			false
		);
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
			uint32_t buffer_size = sizeof(UniformBufferObject);
			UniformBuffer ubo(device, buffer_size);
			ubo.map();

			uniform_buffers.emplace_back(std::move(ubo));
		}
	}

	void create_descriptor_sets() {
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			std::array buffer_info {
				vk::DescriptorBufferInfo {
					.buffer = uniform_buffers[i].get_buffer(),
					.offset = 0,
					.range = sizeof(UniformBufferObject),
				},
			};
			std::array image_info {
				vk::DescriptorImageInfo {
					.sampler = sampler,
					.imageView = gbuffer_albedo->get_image_view(i).get_image_view(),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
				vk::DescriptorImageInfo {
					.sampler = sampler,
					.imageView = gbuffer_normal->get_image_view(i).get_image_view(),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
				vk::DescriptorImageInfo {
					.sampler = sampler,
					.imageView = depth_image->get_image_view(i).get_image_view(),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
			};
			std::array skybox_image_info {
				vk::DescriptorImageInfo {
					.sampler = sampler,
					.imageView = offscreen_framebuffer->get_image_view(i).get_image_view(),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
				vk::DescriptorImageInfo {
					.sampler = sampler,
					.imageView = sky_cubemap.get_image_view().get_image_view(),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
			};
			std::array descriptor_writes {
				vk::WriteDescriptorSet {
					.dstSet = camera_descriptor_sets[i].get_descriptor_set(),
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = buffer_info.data(),
				},
				vk::WriteDescriptorSet {
					.dstSet = shading_descriptor_sets[i].get_descriptor_set(),
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = static_cast<uint32_t>(image_info.size()),
					.descriptorType = vk::DescriptorType::eCombinedImageSampler,
					.pImageInfo = image_info.data()
				},
				vk::WriteDescriptorSet {
					.dstSet = skybox_descriptor_sets[i].get_descriptor_set(),
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = static_cast<uint32_t>(skybox_image_info.size()),
					.descriptorType = vk::DescriptorType::eCombinedImageSampler,
					.pImageInfo = skybox_image_info.data()
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
		device.submit_graphics_onetime(cmd);
	}

	void record_gbuffer_pass(const CommandBuffer &command_buffer) {
		depth_image->get_image(current_frame).as_depth_target(command_buffer);
		gbuffer_albedo->get_image(current_frame).as_color_target(command_buffer);
		gbuffer_normal->get_image(current_frame).as_color_target(command_buffer);

		std::span color_attachments = gbuffer_attachment_layout.get_color_infos({
			{AttachmentLayout::Type::Color0, &gbuffer_albedo->get_image_view(current_frame)},
			{AttachmentLayout::Type::Color1, &gbuffer_normal->get_image_view(current_frame)},
		});

		vk::RenderingInfo rendering_info {
			.renderArea =
				{
					.offset = {0, 0},
					.extent = swapchain.get_extent(),
				},
			.layerCount = 1,
			.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
			.pColorAttachments = color_attachments.data(),
			.pDepthAttachment = &gbuffer_attachment_layout.get_depth_info(
				depth_image->get_image_view(current_frame)
			),
		};

		command_buffer.get_command_buffer().beginRendering(rendering_info);

		command_buffer.get_command_buffer().bindPipeline(
			vk::PipelineBindPoint::eGraphics,
			gbuffer_pipeline->get_pipeline()
		);

		command_buffer.get_command_buffer().bindVertexBuffers(0, *vertex_buffer->get_buffer(), {0});
		command_buffer.get_command_buffer()
			.bindIndexBuffer(*index_buffer->get_buffer(), 0, vk::IndexType::eUint32);

		command_buffer.get_command_buffer().bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			gbuffer_pipeline->get_layout(),
			0,
			*camera_descriptor_sets[current_frame].get_descriptor_set(),
			nullptr
		);

		command_buffer.get_command_buffer()
			.drawIndexed(model.get_indices().size(), 11 * 11 * 11, 0, 0, 0);

		command_buffer.get_command_buffer().endRendering();
	}

	void record_shading_pass(const CommandBuffer &command_buffer) {
		gbuffer_albedo->get_image(current_frame).as_sampled(command_buffer);
		gbuffer_normal->get_image(current_frame).as_sampled(command_buffer);
		depth_image->get_image(current_frame).as_sampled(command_buffer);
		offscreen_framebuffer->get_image(current_frame).as_color_target(command_buffer);

		std::span screen_pass_attachments = shading_attachment_layout.get_color_infos({
			{AttachmentLayout::Type::Color0, &offscreen_framebuffer->get_image_view(current_frame)},
		});
		vk::RenderingInfo light_pass_render_info {
			.renderArea = {.offset = {0, 0}, .extent = swapchain.get_extent()},
			.layerCount = 1,
			.colorAttachmentCount = static_cast<uint32_t>(screen_pass_attachments.size()),
			.pColorAttachments = screen_pass_attachments.data(),
		};
		command_buffer.get_command_buffer().beginRendering(light_pass_render_info);
		command_buffer.get_command_buffer().bindPipeline(
			vk::PipelineBindPoint::eGraphics,
			shading_pipeline->get_pipeline()
		);
		command_buffer.get_command_buffer().bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			shading_pipeline->get_layout(),
			0,
			{
				*camera_descriptor_sets[current_frame].get_descriptor_set(),
				*shading_descriptor_sets[current_frame].get_descriptor_set(),
			},
			nullptr
		);
		command_buffer.get_command_buffer().draw(3, 1, 0, 0);
		command_buffer.get_command_buffer().endRendering();
	}

	void record_skybox_pass(const CommandBuffer &command_buffer) {
		offscreen_framebuffer->get_image(current_frame).as_color_target(command_buffer);
		depth_image->get_image(current_frame).as_depth_target(command_buffer);
		sky_cubemap.get_image().as_color_target(command_buffer);

		std::span skybox_pass_attachments = skybox_attachment_layout.get_color_infos({
			{AttachmentLayout::Type::Color0, &offscreen_framebuffer->get_image_view(current_frame)},
		});
		vk::RenderingInfo skybox_pass_render_info {
			.renderArea = {.offset = {0, 0}, .extent = swapchain.get_extent()},
			.layerCount = 1,
			.colorAttachmentCount = static_cast<uint32_t>(skybox_pass_attachments.size()),
			.pColorAttachments = skybox_pass_attachments.data(),
			.pDepthAttachment = &skybox_attachment_layout.get_depth_info(
				depth_image->get_image_view(current_frame)
			),
		};
		command_buffer.get_command_buffer().beginRendering(skybox_pass_render_info);

		command_buffer.get_command_buffer().bindPipeline(
			vk::PipelineBindPoint::eGraphics,
			skybox_pipeline->get_pipeline()
		);
		command_buffer.get_command_buffer().bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			skybox_pipeline->get_layout(),
			0,
			{
				*camera_descriptor_sets[current_frame].get_descriptor_set(),
				*skybox_descriptor_sets[current_frame].get_descriptor_set(),
			},
			nullptr
		);
		command_buffer.get_command_buffer().draw(3, 1, 0, 0);

		command_buffer.get_command_buffer().endRendering();
	}

	void record_imgui_pass(const CommandBuffer &command_buffer) {
		vk::RenderingAttachmentInfo attachment_info {
			.imageView = offscreen_framebuffer->get_image_view(current_frame).get_image_view(),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eLoad,
			.storeOp = vk::AttachmentStoreOp::eStore,
		};
		command_buffer.get_command_buffer().beginRendering({
			.renderArea = {.offset = {0, 0}, .extent = swapchain.get_extent()},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &attachment_info,
		});
		tramogi::graphics::imgui::render(command_buffer.get_command_buffer());
		command_buffer.get_command_buffer().endRendering();
	}

	void record_command_buffer(uint32_t image_index) {
		CommandBuffer &command_buffer = command_buffers[current_frame];
		command_buffer.begin();
		command_buffer.get_command_buffer().setViewport(
			0,
			vk::Viewport(
				0.0f,
				0.0f,
				swapchain.get_extent().width,
				swapchain.get_extent().height,
				0.0f,
				1.0f
			)
		);
		command_buffer.get_command_buffer().setScissor(
			0,
			vk::Rect2D(vk::Offset2D(0, 0), swapchain.get_extent())
		);

		record_gbuffer_pass(command_buffer);
		record_shading_pass(command_buffer);
		if (show_skybox) {
			record_skybox_pass(command_buffer);
		}
		record_imgui_pass(command_buffer);

		vk::ImageBlit2 blit_info {
			.srcSubresource =
				{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			.srcOffsets =
				std::array {
					vk::Offset3D {0, 0, 0},
					vk::Offset3D {
						static_cast<int32_t>(swapchain.get_extent().width),
						static_cast<int32_t>(swapchain.get_extent().height),
						0
					}
				},
			.dstSubresource =
				{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			.dstOffsets = std::array {
				vk::Offset3D {0, 0, 0},
				vk::Offset3D {
					static_cast<int32_t>(swapchain.get_extent().width),
					static_cast<int32_t>(swapchain.get_extent().height),
					0
				}
			},
		};

		command_buffer.get_command_buffer().blitImage2({
			.srcImage = offscreen_framebuffer->get_image(current_frame).get_image(),
			.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
			.dstImage = swapchain.get_image(image_index).get_image(),
			.dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
			.regionCount = 1,
			.pRegions = &blit_info,
			.filter = vk::Filter::eLinear,
		});

		swapchain.get_image(image_index).as_present_source(command_buffer);

		command_buffer.end();
	}

	void draw_frame(double delta) {
		device.wait_idle(current_frame);
		auto image_index = swapchain.get_next_image(current_frame);

		if (!image_index) {
			recreate_swapchain();
			ImGui::EndFrame();
			return;
		}

		command_buffers[current_frame].get_command_buffer().reset();

		update_uniform_buffer(current_frame, delta);
		record_command_buffer(image_index.value());
		device.reset_fence(current_frame);

		device.submit_graphics(command_buffers[current_frame], current_frame);

		auto present_result = device.present(swapchain, image_index.value(), current_frame);
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
			ImGui::DragFloat3("Light", &world_light_direction.x, 0.1f, 0, 0, "%0.1f");
			ImGui::DragFloat3("Position", &pos_translate.x, 0.1f, 0, 0, "%0.1f");
			ImGui::DragFloat3("Rotation", &rot.x, 0.2f, 0, 360, "%.1f");
			ImGui::End();
		}

		glm::mat4 projection_view = camera.get_projection() * camera.get_view();
		glm::vec2 half_yfov_tan = camera.get_half_fov_tan();

		UniformBufferObject ubo {
			.projection_view = projection_view,
			.inverse_projection_view = glm::inverse(projection_view),

			.camera_position = camera.get_position(),
			.camera_forward = camera.get_forward(),
			.camera_right = camera.get_right() * half_yfov_tan.x,
			.camera_up = camera.get_up() * half_yfov_tan.y,
			.z_near = camera.get_z_near(),
			.z_far = camera.get_z_far(),

			.world_light_direction = world_light_direction,

			.model = glm::rotate(
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
			),
			.model_normal = glm::identity<glm::mat4>(),
			.gbuffer_debug = gbuffer_debug,
		};

		ubo.model_normal = glm::transpose(glm::inverse(ubo.model));

		uniform_buffers[current_image].upload_data(&ubo);
	}

	void recreate_swapchain() {
		Size dimension = window.get_size();
		while (dimension.x == 0 || dimension.y == 0) {
			dimension = window.get_size();
			window.wait_events();
		}

		device.wait_idle(current_frame);

		swapchain.recreate(dimension);

		create_image_resources();
		create_descriptor_sets();

		camera.change_perspective(dimension.x, dimension.y, glm::radians(FOV));

		debug_log("Swapchain resized to {}x{}", dimension.x, dimension.y);
	}

	void create_cubemap() {
		std::array<ImageData, 6> skybox;
		skybox[0].load_from_file("textures/skybox/px.png");
		skybox[1].load_from_file("textures/skybox/nx.png");
		skybox[2].load_from_file("textures/skybox/py.png");
		skybox[3].load_from_file("textures/skybox/ny.png");
		skybox[4].load_from_file("textures/skybox/pz.png");
		skybox[5].load_from_file("textures/skybox/nz.png");

		StagingBuffer stage(device, skybox[0].get_size() * 6);
		stage.map();
		for (auto i = 0; i < 6; ++i) {
			const auto &image_data = skybox[i];
			memcpy(
				static_cast<uint8_t *>(stage.get_mapped_memory()) + (i * image_data.get_size()),
				image_data.get_data(),
				image_data.get_size()
			);
		}

		CommandBuffer cmd = device.allocate_command_buffer();
		cmd.begin_onetime();

		vk::BufferImageCopy2 region {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource =
				{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 6,
				},
			.imageOffset = {0, 0, 0},
			.imageExtent = {
				.width = static_cast<uint32_t>(skybox[0].get_width()),
				.height = static_cast<uint32_t>(skybox[0].get_height()),
				.depth = 1,
			},
		};

		cmd.get_command_buffer().copyBufferToImage2({
			.srcBuffer = stage.get_buffer(),
			.dstImage = sky_cubemap.get_image().get_image(),
			.dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
			.regionCount = 1,
			.pRegions = &region,
		});

		cmd.end();
		device.submit_graphics_onetime(cmd);
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
