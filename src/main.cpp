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
#include "tramogi/core/logging/logging.h"
#include "tramogi/core/types.h"
#include "tramogi/graphics/buffer.h"
#include "tramogi/graphics/imgui/imgui_loader.h"
#include "tramogi/input/keyboard.h"
#include "tramogi/input/mouse.h"
#include "tramogi/platform/window.h"

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;

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
		  device(physical_device, instance), swapchain(physical_device, device, window.get_size()),
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

	Swapchain swapchain;

	vk::raii::DescriptorSetLayout descriptor_set_layout = nullptr;
	vk::raii::PipelineLayout pipeline_layout = nullptr;
	vk::raii::Pipeline graphics_pipeline = nullptr;

	std::vector<CommandBuffer> command_buffers;

	std::unique_ptr<VertexBuffer> vertex_buffer;
	std::unique_ptr<IndexBuffer> index_buffer;
	std::vector<UniformBuffer> uniform_buffers;

	vk::raii::DescriptorPool descriptor_pool = nullptr;
	std::vector<vk::raii::DescriptorSet> descriptor_sets;

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
		dynamic_render_info.pColorAttachmentFormats = &swapchain.get_format();
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
			.pColorAttachmentFormats = &swapchain.get_format(),
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

	void create_depth_resources() {
		depth_image = std::make_unique<ImageViewPair<DepthImage>>(
			physical_device,
			device,
			swapchain.get_extent().width,
			swapchain.get_extent().height,
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
			std::array descriptor_writes {
				vk::WriteDescriptorSet {
					.dstSet = descriptor_sets[i],
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = &buffer_info,
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

	void record_command_buffer(uint32_t image_index) {
		CommandBuffer &command_buffer = command_buffers[current_frame];
		command_buffer.begin();

		swapchain.get_image(image_index).as_attachment(command_buffer);
		depth_image->get_image().as_depth_target(command_buffers[current_frame]);

		vk::ClearValue clear_color = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue clear_depth = vk::ClearDepthStencilValue(1.0f, 0);
		vk::RenderingAttachmentInfo attachment_info {
			.imageView = swapchain.get_image_view(image_index).get_image_view(),
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
					.extent = swapchain.get_extent(),
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

		swapchain.get_image(image_index).as_present_source(command_buffer);

		command_buffer.end();
	}

	void draw_frame(double delta) {
		device.wait_idle(current_frame);
		auto image_index = swapchain.get_next_image(current_frame);

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
			.pSwapchains = &*swapchain.get_swapchain(),
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

		swapchain.recreate(dimension);

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
