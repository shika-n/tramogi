#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <expected>
#include <format>
#include <functional>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>
#include <initializer_list>
#include <limits>
#include <memory>
#include <print>
#include <set>
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

#include "core/heightmap.h"
#include "engine/camera.h"
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
#include "renderer/primitives/cube.h"
#include "renderer/primitives/heightmap_terrain.h"
#include "tramogi/core/io/file.h"
#include "tramogi/core/io/image_data.h"
#include "tramogi/core/logging/logging.h"
#include "tramogi/core/pointers.h"
#include "tramogi/core/types.h"
#include "tramogi/graphics/buffer.h"
#include "tramogi/graphics/imgui/imgui_loader.h"
#include "tramogi/input/keyboard.h"
#include "tramogi/input/mouse.h"
#include "tramogi/platform/window.h"
#include "tramogi/renderer/mesh.h"
#include "tramogi/renderer/model.h"
#include "tramogi/renderer/primitives/basic_vertex.h"

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;
constexpr float FOV = 90;
constexpr float SHADOW_RESOLUTION = 4096;

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

using namespace tramogi::core;
using namespace tramogi::engine;
using namespace tramogi::graphics;
using namespace tramogi::input;
using namespace tramogi::platform;

using namespace tramogi::core::logging;

using tramogi::renderer::Mesh;
using tramogi::renderer::Model;
using tramogi::renderer::primitives::BasicVertex;
using tramogi::renderer::primitives::CubeMesh;
using tramogi::renderer::primitives::HeightmapTerrainMesh;

constexpr uint16_t UBO_DEBUG_OPTIONS_GBUFFER_BIT = 0x7;
constexpr uint16_t UBO_DEBUG_OPTIONS_FOG_BIT = 1 << 3;
constexpr uint16_t UBO_DEBUG_OPTIONS_SPECULAR_BIT = 1 << 4;
constexpr uint16_t UBO_DEBUG_OPTIONS_SHADOW_BIT = 0x7 << 5;
constexpr uint16_t UBO_DEBUG_OPTIONS_SHADOW_DITHER_BIT = 0x1 << 8;
constexpr uint16_t UBO_DEBUG_OPTIONS_SHADOW_RANDOM_POISSON_BIT = 0x1 << 9;
constexpr uint16_t UBO_DEBUG_OPTIONS_NORMAL_MAP = 0x1 << 10;
struct CameraUniformBufferObject {
	glm::mat4 projection_view;
	glm::mat4 inverse_projection_view;

	alignas(16) glm::vec3 camera_position;
	alignas(16) glm::vec3 camera_forward;
	alignas(16) glm::vec3 camera_right;
	alignas(16) glm::vec3 camera_up;
	float z_near;
	float z_far;

	alignas(16) glm::vec3 world_light_direction;
	alignas(16) glm::mat4 shadow_projection_view;

	// 3 bits - GBuffer
	//   0: Shaded
	//   1: Albedo
	//   2: Normal
	//   3: Depth
	//   4: Position
	// 1 bit  - Specular
	// 1 bit  - Fog
	// 3 bits - Shadow
	//   0: Poisson + Hardware PCF (x16)
	//   1: Poisson PCF (x16)
	//   2: Hardware PCF (x16)
	//   3: Poisson PCF (x8)
	//   4: Hardware PCF (x4)
	//   5: No PCF (x1)
	//   6: No shadow
	// 1 bit - Dither
	// 1 bit - Random Poisson
	// 1 bit - Normal map
	uint16_t debug_options;
};

struct ObjectTransformMatrices {
	glm::mat4 transform;
	glm::mat4 normal;

	alignas(16) float metalness;
};
struct ObjectUniformBufferObject {
	ObjectTransformMatrices transform_matrices[100];
};

struct PushConstantData {
	uint32_t model_id;
	uint32_t texture_id;
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
		  object_descriptor_layout(device, object_binds),
		  sky_cubemap(physical_device, device, 512, 512, Format::RGBA8Srgb, false),
		  camera(WIDTH, HEIGHT, glm::radians(FOV)), shadow_camera(200, 200, 0, 1000) {
		camera.set_position({0, 0, -5});
		shadow_camera.set_position({0, 2, 0});
	}

	~ProjectSkyHigh() {
		debug_log("Engine dtor called");
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

	std::vector<UniquePtr<VertexBuffer>> vertex_buffers;
	std::vector<UniquePtr<IndexBuffer>> index_buffers;
	std::vector<UniformBuffer> uniform_buffers;
	std::vector<UniformBuffer> object_uniform_buffers;

	DescriptorPool descriptor_pool;
	DescriptorLayout camera_descriptor_layout;
	DescriptorLayout shading_descriptor_layout;
	DescriptorLayout skybox_descriptor_layout;
	DescriptorLayout object_descriptor_layout;
	std::vector<DescriptorSet> camera_descriptor_sets;
	std::vector<DescriptorSet> shading_descriptor_sets;
	std::vector<DescriptorSet> skybox_descriptor_sets;
	std::vector<DescriptorSet> object_descriptor_sets;

	UniquePtr<Pipeline> gbuffer_pipeline;
	UniquePtr<Pipeline> shadow_pipeline;
	UniquePtr<Pipeline> shading_pipeline;
	UniquePtr<Pipeline> skybox_pipeline;
	UniquePtr<Pipeline> wireframe_pipeline;

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
			.count = 5,
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

	constexpr static std::array object_binds = {
		DescriptorLayoutBinding {
			.location = 0,
			.stage = DescriptorLayoutBinding::Stage::VertexFragment,
			.type = DescriptorLayoutBinding::Type::UniformbBuffer,
		},
		DescriptorLayoutBinding {
			.location = 1,
			.count = 2,
			.stage = DescriptorLayoutBinding::Stage::Fragment,
			.type = DescriptorLayoutBinding::Type::CombinedSampler,
		},
	};

	UniquePtr<InFlightSet<DepthImage>> depth_image;
	UniquePtr<InFlightSet<Image>> gbuffer_albedo;
	UniquePtr<InFlightSet<Image>> gbuffer_normal;
	UniquePtr<InFlightSet<Image>> offscreen_framebuffer;
	UniquePtr<InFlightSet<DepthImage>> shadow_depth_image;
	UniquePtr<InFlightSet<Image>> object_id_image;

	UniquePtr<StagingBuffer> position_staging;
	UniquePtr<StagingBuffer> object_id_staging;

	AttachmentLayout gbuffer_attachment_layout;
	AttachmentLayout shading_attachment_layout;
	AttachmentLayout skybox_attachment_layout;
	AttachmentLayout shadow_attachment_layout;
	AttachmentLayout wireframe_attachment_layout;

	vk::raii::Sampler sampler = nullptr;
	vk::raii::Sampler depth_sampler = nullptr;

	ImageViewPair<CubeMapImage> sky_cubemap;
	UniquePtr<ImageViewPair<Image>> terrain_texture;
	UniquePtr<ImageViewPair<Image>> terrain_normal_texture;

	uint32_t current_frame = 0;

	Camera camera;
	OrthoCamera shadow_camera;

	Keyboard key_input;
	Mouse mouse_input;

	CubeMesh cube_mesh;
	UniquePtr<HeightmapTerrainMesh> terrain_mesh;
	Mesh bunny_mesh;
	Mesh dragon_mesh;
	Mesh lucy_mesh;

	std::vector<Model> models;
	std::set<uint32_t> id_cache;

	uint32_t current_picked_id = 0;

	bool is_imgui_visible = true;

	glm::vec2 directional_light_angles = glm::vec2(30.0f, 60.0f);

	uint16_t debug_options = 0;
	bool skybox_enabled = true;
	bool wireframe_enabled = false;
	bool wireframe_selected_only = true;
	bool auto_update_enabled = true;

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
			.minLod = 0.0f,
			.maxLod = vk::LodClampNone,
		};
		sampler = vk::raii::Sampler(device.get_device(), sampler_info);
		vk::SamplerCreateInfo depth_sampler_info {
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eLinear,
			.addressModeU = vk::SamplerAddressMode::eRepeat,
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.addressModeW = vk::SamplerAddressMode::eRepeat,
			.mipLodBias = 0.0f,
			.anisotropyEnable = vk::True,
			.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
			.compareEnable = vk::True,
			.compareOp = vk::CompareOp::eLess,
		};
		depth_sampler = vk::raii::Sampler(device.get_device(), depth_sampler_info);
	}
	void init_vulkan() {
		create_image_resources();
		create_shadow_depth_image();

		create_graphics_pipeline();
		create_shading_pipeline();
		create_skybox_pipeline();
		create_shadow_pipeline();
		create_wireframe_pipeline();

		create_texture_sampler();

		allocate_descriptor_sets();
		create_descriptor_sets();
		create_command_buffers();

		create_cubemap();

		load_heightmap();

		load_obj_model();

		create_vertex_buffer();
		create_index_buffer();
		create_uniform_buffers();
		update_ubo_descriptor_sets();

		create_models();
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

	void prepare_imgui_components() {
		ImGui::Begin("Properties");
		ImGui::Text(
			"Position %.3f %.3f %.3f",
			camera.get_position().x,
			camera.get_position().y,
			camera.get_position().z
		);
		ImGui::Text("Mouse %.3f %.3f", mouse_input.get_x(), mouse_input.get_y());

		ImGui::Text(
			"Light %.3f %.3f %.3f",
			shadow_camera.get_forward().x,
			shadow_camera.get_forward().y,
			shadow_camera.get_forward().z
		);

		if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat(
				"Yaw",
				&directional_light_angles.y,
				0.25f,
				0,
				360,
				"%0.01f",
				ImGuiSliderFlags_WrapAround
			);
			ImGui::DragFloat("Pitch", &directional_light_angles.x, 0.05f, -90, 90, "%0.01f");
		}

		static int32_t gbuffer_debug = 0;
		if (ImGui::CollapsingHeader("Geometry Buffer", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::RadioButton("Shaded", &gbuffer_debug, 0);
			ImGui::SameLine();
			ImGui::RadioButton("Albedo", &gbuffer_debug, 1);
			ImGui::RadioButton("Normal", &gbuffer_debug, 2);
			ImGui::SameLine();
			ImGui::RadioButton("Depth", &gbuffer_debug, 3);
			ImGui::RadioButton("Position", &gbuffer_debug, 4);
		}

		static int32_t selected_shadow_type = 0;
		static bool random_poisson = false;
		static bool dither = false;
		if (ImGui::CollapsingHeader("Shadow")) {
			ImGui::RadioButton("1. Poisson + hardware PCF (x16)", &selected_shadow_type, 0);
			ImGui::RadioButton("2. Poisson PCF (x16)", &selected_shadow_type, 1);
			ImGui::RadioButton("1. Hardware PCF (x16)", &selected_shadow_type, 2);
			ImGui::RadioButton("3. Poisson PCF (x8)", &selected_shadow_type, 3);
			ImGui::RadioButton("4. Hardware PCF (x4)", &selected_shadow_type, 4);
			ImGui::RadioButton("5. No PCF (x1)", &selected_shadow_type, 5);
			ImGui::RadioButton("6. No shadow", &selected_shadow_type, 6);

			ImGui::Separator();
			ImGui::Checkbox("Dither", &dither);
			ImGui::Checkbox("Random poisson disk", &random_poisson);
		}

		static bool specular_enabled = true;
		static bool fog_enabled = true;
		static bool normal_map_enabled = true;
		if (ImGui::CollapsingHeader("Misc.")) {
			ImGui::Checkbox("Fog", &fog_enabled);
			ImGui::Checkbox("Skybox", &skybox_enabled);
			ImGui::Checkbox("Specular", &specular_enabled);
			ImGui::Checkbox("Apply normal map", &normal_map_enabled);
			ImGui::Checkbox("Wireframe", &wireframe_enabled);
			ImGui::Checkbox("Wireframe Selected Only", &wireframe_selected_only);
			ImGui::Checkbox("Enable auto update", &auto_update_enabled);
		}

		if (ImGui::CollapsingHeader("Scene")) {
			std::array mesh_names = {
				"Cube",
				"Stanford Bunny",
				"Stanford Dragon",
				"Stanford Lucy",
			};

			std::array mesh_ptrs = {
				static_cast<Mesh *>(&cube_mesh),
				&bunny_mesh,
				&dragon_mesh,
				&lucy_mesh,
			};
			static int32_t selected_mesh_index = 0;
			if (ImGui::BeginCombo("Mesh", mesh_names[selected_mesh_index])) {
				for (size_t i = 0; i < mesh_names.size(); ++i) {
					if (ImGui::Selectable(mesh_names[i], false)) {
						selected_mesh_index = i;
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::Button("Add Object")) {
				uint32_t next_id = 0;
				for (size_t i = 0; i <= id_cache.size(); ++i) {
					if (id_cache.count(i) == 0) {
						next_id = i;
						break;
					}
				}

				Mesh *mesh_ptr = mesh_ptrs[selected_mesh_index];

				Model model(next_id, mesh_ptr);
				model.set_name(
					std::format("Object {} - {}", model.get_id(), mesh_names[selected_mesh_index])
				);
				if (mesh_ptr == &bunny_mesh) {
					model.set_scale(glm::vec3(20));
				} else if (mesh_ptr == &lucy_mesh) {
					model.set_scale(glm::vec3(5));
				}

				models.emplace_back(std::move(model));
				id_cache.insert(next_id);
			}
		}

		debug_options = (debug_options & (0xFFFF ^ UBO_DEBUG_OPTIONS_GBUFFER_BIT)) |
						(gbuffer_debug & UBO_DEBUG_OPTIONS_GBUFFER_BIT);
		debug_options = (debug_options & (0xFFFF ^ UBO_DEBUG_OPTIONS_FOG_BIT)) |
						(!fog_enabled * UBO_DEBUG_OPTIONS_FOG_BIT);
		debug_options = (debug_options & (0xFFFF ^ UBO_DEBUG_OPTIONS_SPECULAR_BIT)) |
						(!specular_enabled * UBO_DEBUG_OPTIONS_SPECULAR_BIT);
		debug_options = (debug_options & (0xFFFF ^ UBO_DEBUG_OPTIONS_SHADOW_BIT)) |
						(((selected_shadow_type & 0x7) << 5) & UBO_DEBUG_OPTIONS_SHADOW_BIT);
		debug_options = (debug_options & (0xFFFF ^ UBO_DEBUG_OPTIONS_SHADOW_DITHER_BIT)) |
						(dither * UBO_DEBUG_OPTIONS_SHADOW_DITHER_BIT);
		debug_options = (debug_options & (0xFFFF ^ UBO_DEBUG_OPTIONS_SHADOW_RANDOM_POISSON_BIT)) |
						(random_poisson * UBO_DEBUG_OPTIONS_SHADOW_RANDOM_POISSON_BIT);
		debug_options = (debug_options & (0xFFFF ^ UBO_DEBUG_OPTIONS_NORMAL_MAP)) |
						(!normal_map_enabled * UBO_DEBUG_OPTIONS_NORMAL_MAP);

		ImGui::End();

		ImGui::Begin("Objects");
		if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (auto &model : models) {
				ImGuiTreeNodeFlags selected_flag = ImGuiTreeNodeFlags_None;
				if (model.get_id() == current_picked_id) {
					selected_flag |= ImGuiTreeNodeFlags_Selected;
				}
				ImGui::TreeNodeEx(model.get_name().data(), ImGuiTreeNodeFlags_Leaf | selected_flag);
				if (ImGui::IsItemClicked()) {
					current_picked_id = model.get_id();
				}
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}
		ImGui::End();

		ImGui::Begin("Object Property");
		for (auto &model : models) {
			if (current_picked_id != std::numeric_limits<uint32_t>::max() &&
				model.get_id() == current_picked_id) {
				ImGui::Text("Selected: [%i] %s", model.get_id(), model.get_name().data());

				static char name[33] = "\0";
				memset(name, '\0', 33);
				model.get_name().copy(name, 32);
				ImGui::InputText("Name", name, 32);
				model.set_name(name);

				static glm::vec3 position;
				position = model.get_position();
				ImGui::DragFloat3("Position", &position.x, 0.1f);
				model.set_position(position);

				static glm::vec3 scale;
				scale = model.get_scale();
				ImGui::DragFloat3("Scale", &scale.x, 0.1f);
				model.set_scale(scale);

				static glm::vec3 rotation;
				rotation = glm::degrees(glm::eulerAngles(model.get_orientation()));
				ImGui::DragFloat3("Rotation", &rotation.x, 0.1f);
				model.set_orientation(glm::quat(glm::radians(rotation)));

				static float metalness;
				metalness = model.get_metalness();
				ImGui::SliderFloat("Metalness", &metalness, 0.0, 1.0);
				model.set_metalness(metalness);

				if (ImGui::Button("Set as Camera POI")) {
					camera.set_point_of_interest(model.get_position());
				}
				if (ImGui::Button("Delete")) {
					auto result = std::ranges::find_if(models, [&model](auto &value) -> bool {
						return &model == &value;
					});
					if (result != models.cend()) {
						id_cache.erase(result->get_id());
						models.erase(result);
					}
				}
			}
		}
		ImGui::End();
	}

	void main_loop() {
		debug_log("Starting main loop");

		auto last_time = std::chrono::high_resolution_clock().now();
		uint32_t frames = 0;
		double timer = 0;
		bool print_fps = false;

		glm::vec2 last_pos;
		bool drag_first_frame = true;

		double time = 0.0;

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

			copy_object_image();

			// TODO: Fix with better code
			// Start pressing inside imgui and releasing just a bit over the border will count
			// as a click
			if (!ImGui::IsWindowHovered(
					ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
				)) {
				static float pressed_x = -1;
				static float pressed_y = -1;

				bool drag_rotating = mouse_input.is_pressed(MouseButton::Middle) ||
									 (key_input.is_pressed(Key::Control) &&
									  mouse_input.is_pressed(MouseButton::Right));
				if (drag_rotating) {
					if (!drag_first_frame) {
						float delta_x = mouse_input.get_x() - last_pos.x;
						float delta_y = mouse_input.get_y() - last_pos.y;
						if (key_input.is_pressed(Key::Shift)) {
							camera.set_point_of_interest(
								camera.get_point_of_interest() +
								(camera.get_up() * delta_y + camera.get_right() * -delta_x) * 0.2f
							);
						} else {
							camera.rotate_to_poi(
								glm::radians(1.0f * delta_y),
								glm::radians(1.0f * delta_x)
							);
						}
					}
					last_pos = glm::vec2(mouse_input.get_x(), mouse_input.get_y());
				}

				if (mouse_input.is_pressed(MouseButton::Left) && pressed_x < 0.0f &&
					pressed_y < 0.0f) {
					pressed_x = mouse_input.get_x();
					pressed_y = mouse_input.get_y();
				} else if (!mouse_input.is_pressed(MouseButton::Left)) {
					if (pressed_x > 0.0f && pressed_y > 0.0f &&
						std::abs(mouse_input.get_x() - pressed_x) < 16 &&
						std::abs(mouse_input.get_y() - pressed_y) < 16) {
						uint32_t pixel_pos = static_cast<uint32_t>(
							mouse_input.get_x() + mouse_input.get_y() * swapchain.get_extent().width
						);
						if (pixel_pos <
							swapchain.get_extent().width * swapchain.get_extent().height) {
							uint32_t object_id = *(
								static_cast<uint32_t *>(object_id_staging->get_mapped_memory()) +
								pixel_pos
							);
							current_picked_id = object_id;
						}
					}
					pressed_x = -1;
					pressed_y = -1;
				}

				drag_first_frame = !drag_rotating;
			}

			camera.update_view();
			// shadow_camera.set_position(camera.get_position());
			shadow_camera.set_orientation(
				glm::quat(glm::vec3(glm::radians(directional_light_angles), 0))
			);
			shadow_camera.update_view();

			if (is_imgui_visible) {
				prepare_imgui_components();
			}

			if (auto_update_enabled) {
				for (auto &model : models) {
					if (model.get_mesh() == &bunny_mesh) {
						model.set_position({
							model.get_position().x,
							std::max(std::sin((time + model.get_id()) * 5.0f) * 2.0f, 0.0) - 0.5f,
							model.get_position().z,
						});
					} else if (model.get_mesh() == &cube_mesh) {
						model.set_orientation(
							model.get_orientation() *
							glm::angleAxis(
								glm::radians(90.0f) * static_cast<float>(delta),
								glm::vec3(0, 1, 0)
							) *
							glm::angleAxis(
								glm::radians(30.0f) * static_cast<float>(delta),
								model.get_orientation() * glm::vec3(1, 0, 0)
							)
						);
					}
				}
			}

			draw_frame(delta);

			++frames;
			time += delta;
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
		auto shader_code_result = read_shader_file("shaders/gbuffer.spv");
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
		vertex_descriptor.add_attributes({
			.location = 3,
			.format = Format::Float3,
			.offset = offsetof(BasicVertex, tangent),
		});
		vertex_descriptor.add_attributes({
			.location = 4,
			.format = Format::Float2,
			.offset = offsetof(BasicVertex, uv),
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
		gbuffer_attachment_layout.add_attachment(
			AttachmentLayout::Type::Color2,
			object_id_image->get_image(0).get_format()
		);
		gbuffer_attachment_layout.set_clear_value(
			AttachmentLayout::Type::Color2,
			{std::numeric_limits<uint32_t>::max()}
		);

		gbuffer_pipeline = std::make_unique<Pipeline>(
			device,
			shader_module,
			std::initializer_list {
				&camera_descriptor_layout,
				&object_descriptor_layout,
			},
			vertex_descriptor,
			gbuffer_attachment_layout,
			Pipeline::Option {
				.depth_test = Pipeline::Option::DepthTest::DepthTestAndWrite,
				.depth_compare = Pipeline::Option::DepthCompare::Less,
			}
		);
	}

	void create_shading_pipeline() {
		auto shader_code_result = read_shader_file("shaders/shading.spv");
		if (!shader_code_result) {
			throw std::runtime_error(shader_code_result.error());
		}
		auto shader_code = shader_code_result.value();

		Shader shader_module(device, shader_code);
		shader_module.add_vertex_stage("screen_vert");
		shader_module.add_fragment_stage("shading_frag");

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
			Pipeline::Option {
				.depth_test = Pipeline::Option::DepthTest::None,
			}
		);
	}

	void create_skybox_pipeline() {
		auto shader_code_result = read_shader_file("shaders/skybox.spv");
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
			Pipeline::Option {
				.depth_test = Pipeline::Option::DepthTest::DepthTestOnly,
				.depth_compare = Pipeline::Option::DepthCompare::LessOrEqual,
			}
		);
	}

	void create_shadow_pipeline() {
		auto shader_code_result = read_shader_file("shaders/gbuffer.spv");
		if (!shader_code_result) {
			throw std::runtime_error(shader_code_result.error());
		}
		auto shader_code = shader_code_result.value();

		Shader shader_module(device, shader_code);
		shader_module.add_vertex_stage("shadow_vert");

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
		vertex_descriptor.add_attributes({
			.location = 3,
			.format = Format::Float2,
			.offset = offsetof(BasicVertex, uv),
		});

		shadow_attachment_layout.add_attachment(
			AttachmentLayout::Type::Depth,
			shadow_depth_image->get_image(0).get_format()
		);

		shadow_pipeline = std::make_unique<Pipeline>(
			device,
			shader_module,
			std::initializer_list {
				&camera_descriptor_layout,
				&object_descriptor_layout,
			},
			vertex_descriptor,
			shadow_attachment_layout,
			Pipeline::Option {
				.depth_test = Pipeline::Option::DepthTest::DepthTestAndWrite,
				.depth_compare = Pipeline::Option::DepthCompare::Less,
				.depth_bias =
					Pipeline::Option::DepthBias {
						.bias = 1.5f,
						.slope = 2.5f,
					},
				.cull_mode = Pipeline::Option::CullMode::None,
			}
		);
	}

	void create_wireframe_pipeline() {
		auto shader_code_result = read_shader_file("shaders/gbuffer.spv");
		if (!shader_code_result) {
			throw std::runtime_error(shader_code_result.error());
		}
		auto shader_code = shader_code_result.value();

		Shader shader_module(device, shader_code);
		shader_module.add_vertex_stage("basic_vert");
		shader_module.add_fragment_stage("solid_frag");

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
		vertex_descriptor.add_attributes({
			.location = 3,
			.format = Format::Float2,
			.offset = offsetof(BasicVertex, uv),
		});
		wireframe_attachment_layout.add_attachment(
			AttachmentLayout::Type::Depth,
			depth_image->get_image(0).get_format()
		);
		wireframe_attachment_layout.add_attachment(
			AttachmentLayout::Type::Color0,
			offscreen_framebuffer->get_image(0).get_format()
		);

		wireframe_attachment_layout.set_load_operation(
			AttachmentLayout::Type::Depth,
			AttachmentLayout::LoadOperation::Load
		);
		wireframe_attachment_layout.set_load_operation(
			AttachmentLayout::Type::Color0,
			AttachmentLayout::LoadOperation::Load
		);

		wireframe_pipeline = std::make_unique<Pipeline>(
			device,
			shader_module,
			std::initializer_list {
				&camera_descriptor_layout,
				&object_descriptor_layout,
			},
			vertex_descriptor,
			wireframe_attachment_layout,
			Pipeline::Option {
				.depth_test = Pipeline::Option::DepthTest::DepthTestOnly,
				.depth_compare = Pipeline::Option::DepthCompare::LessOrEqual,
				.polygon_mode = Pipeline::Option::PolygonMode::Wireframe,
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
		object_id_image = std::make_unique<InFlightSet<Image>>(
			physical_device,
			device,
			swapchain.get_extent().width,
			swapchain.get_extent().height,
			Format::R32UInt,
			Image::Usage::SampledColorTarget,
			false
		);

		object_id_staging = std::make_unique<StagingBuffer>(
			device,
			swapchain.get_extent().width * swapchain.get_extent().height * sizeof(uint32_t)
		);
		object_id_staging->map();
	}

	void create_shadow_depth_image() {
		shadow_depth_image = std::make_unique<InFlightSet<DepthImage>>(
			physical_device,
			device,
			SHADOW_RESOLUTION,
			SHADOW_RESOLUTION,
			false
		);
	}

	void create_command_buffers() {
		command_buffers = device.allocate_command_buffers(MAX_FRAMES_IN_FLIGHT);
	}

	void create_vertex_buffer() {
		std::array<Mesh *, 5> meshes = {
			static_cast<Mesh *>(terrain_mesh.get()),
			&cube_mesh,
			&bunny_mesh,
			&dragon_mesh,
			&lucy_mesh,
		};

		for (Mesh *mesh : meshes) {
			auto buffer_size = sizeof(mesh->get_vertices()[0]) * mesh->get_vertices().size();

			StagingBuffer staging_buffer(device, buffer_size);
			staging_buffer.map();
			staging_buffer.upload_data(mesh->get_vertices().data());
			staging_buffer.unmap();

			VertexBuffer *vertex_buffer =
				vertex_buffers.emplace_back(std::make_unique<VertexBuffer>(device, buffer_size))
					.get();
			copy_buffer(staging_buffer.get_buffer(), vertex_buffer->get_buffer(), buffer_size);

			mesh->set_vertex_buffer(vertex_buffer);
		}
	}

	void create_index_buffer() {
		std::array<Mesh *, 5> meshes = {
			static_cast<Mesh *>(terrain_mesh.get()),
			&cube_mesh,
			&bunny_mesh,
			&dragon_mesh,
			&lucy_mesh
		};

		for (Mesh *mesh : meshes) {
			auto buffer_size = sizeof(mesh->get_indices()[0]) * mesh->get_indices().size();

			StagingBuffer staging_buffer(device, buffer_size);
			staging_buffer.map();
			staging_buffer.upload_data(mesh->get_indices().data());
			staging_buffer.unmap();

			IndexBuffer *index_buffer =
				index_buffers.emplace_back(std::make_unique<IndexBuffer>(device, buffer_size))
					.get();
			copy_buffer(staging_buffer.get_buffer(), index_buffer->get_buffer(), buffer_size);

			mesh->set_index_buffer(index_buffer);
		}
	}

	void create_uniform_buffers() {
		uniform_buffers.clear();
		object_uniform_buffers.clear();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			UniformBuffer ubo(device, sizeof(CameraUniformBufferObject));
			ubo.map();
			uniform_buffers.emplace_back(std::move(ubo));

			UniformBuffer object_ubo(device, sizeof(ObjectUniformBufferObject));
			object_ubo.map();
			object_uniform_buffers.emplace_back(std::move(object_ubo));
		}
	}

	void allocate_descriptor_sets() {
		camera_descriptor_sets = device.allocate_descriptor_sets(
			descriptor_pool,
			camera_descriptor_layout,
			MAX_FRAMES_IN_FLIGHT
		);
		object_descriptor_sets = device.allocate_descriptor_sets(
			descriptor_pool,
			object_descriptor_layout,
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
	}

	void update_ubo_descriptor_sets() {
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			std::array buffer_info {
				vk::DescriptorBufferInfo {
					.buffer = uniform_buffers[i].get_buffer(),
					.offset = 0,
					.range = sizeof(CameraUniformBufferObject),
				},
			};
			std::array object_buffer_info {
				vk::DescriptorBufferInfo {
					.buffer = object_uniform_buffers[i].get_buffer(),
					.offset = 0,
					.range = sizeof(ObjectUniformBufferObject),
				},
			};
			std::array object_texture_info {
				vk::DescriptorImageInfo {
					.sampler = sampler,
					.imageView = terrain_texture->get_image_view().get_image_view(),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
				vk::DescriptorImageInfo {
					.sampler = sampler,
					.imageView = terrain_normal_texture->get_image_view().get_image_view(),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
			};
			std::array descriptor_writes {
				vk::WriteDescriptorSet {
					.dstSet = camera_descriptor_sets[i].get_descriptor_set(),
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = buffer_info.size(),
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = buffer_info.data(),
				},
				vk::WriteDescriptorSet {
					.dstSet = object_descriptor_sets[i].get_descriptor_set(),
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = object_buffer_info.size(),
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = object_buffer_info.data(),
				},
				vk::WriteDescriptorSet {
					.dstSet = object_descriptor_sets[i].get_descriptor_set(),
					.dstBinding = 1,
					.dstArrayElement = 0,
					.descriptorCount = object_texture_info.size(),
					.descriptorType = vk::DescriptorType::eCombinedImageSampler,
					.pImageInfo = object_texture_info.data(),
				},
			};
			device.get_device().updateDescriptorSets(descriptor_writes, {});
		}
	}

	void create_descriptor_sets() {
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
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
				vk::DescriptorImageInfo {
					.sampler = depth_sampler,
					.imageView = shadow_depth_image->get_image_view(i).get_image_view(),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
				vk::DescriptorImageInfo {
					.sampler = sampler,
					.imageView = shadow_depth_image->get_image_view(i).get_image_view(),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
				vk::DescriptorImageInfo {
					.sampler = sampler,
					.imageView = sky_cubemap.get_image_view().get_image_view(),
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

	void copy_buffer(
		const vk::raii::Buffer &src,
		const vk::raii::Buffer &dst,
		vk::DeviceSize size
	) {
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

	void record_draw_objects(const CommandBuffer &command_buffer, bool selected_only) {
		PushConstantData push_constant_data;
		vk::PushConstantsInfo push_contant_info {
			.layout = gbuffer_pipeline->get_layout(),
			.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			.size = sizeof(PushConstantData),
			.pValues = &push_constant_data,
		};

		for (size_t i = 0; i < models.size(); ++i) {
			const auto &model = models[i];
			if (model.get_mesh() == nullptr ||
				(selected_only && current_picked_id != model.get_id())) {
				continue;
			}

			push_constant_data.model_id = model.get_id();

			if (model.get_mesh() == terrain_mesh.get()) { // Terrain
				push_constant_data.texture_id = 1;
			} else {
				push_constant_data.texture_id = 0;
			}

			command_buffer.get_command_buffer()
				.bindVertexBuffers(0, *model.get_mesh()->get_vertex_buffer()->get_buffer(), {0});
			command_buffer.get_command_buffer().bindIndexBuffer(
				*model.get_mesh()->get_index_buffer()->get_buffer(),
				0,
				vk::IndexType::eUint32
			);

			command_buffer.get_command_buffer().pushConstants2(push_contant_info);
			command_buffer.get_command_buffer()
				.drawIndexed(model.get_mesh()->get_indices().size(), 1, 0, 0, 0);
		}
	}

	void record_gbuffer_pass(const CommandBuffer &command_buffer) {
		depth_image->get_image(current_frame).as_depth_target(command_buffer);
		gbuffer_albedo->get_image(current_frame).as_color_target(command_buffer);
		gbuffer_normal->get_image(current_frame).as_color_target(command_buffer);
		object_id_image->get_image(current_frame).as_color_target(command_buffer);
		terrain_texture->get_image().as_sampled(command_buffer);

		std::span color_attachments = gbuffer_attachment_layout.get_color_infos({
			{AttachmentLayout::Type::Color0, &gbuffer_albedo->get_image_view(current_frame)},
			{AttachmentLayout::Type::Color1, &gbuffer_normal->get_image_view(current_frame)},
			{AttachmentLayout::Type::Color2, &object_id_image->get_image_view(current_frame)},
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

		command_buffer.get_command_buffer().bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			gbuffer_pipeline->get_layout(),
			0,
			{
				*camera_descriptor_sets[current_frame].get_descriptor_set(),
				*object_descriptor_sets[current_frame].get_descriptor_set(),
			},
			nullptr
		);

		record_draw_objects(command_buffer, false);

		command_buffer.get_command_buffer().endRendering();
	}

	void record_shading_pass(const CommandBuffer &command_buffer) {
		gbuffer_albedo->get_image(current_frame).as_sampled(command_buffer);
		gbuffer_normal->get_image(current_frame).as_sampled(command_buffer);
		depth_image->get_image(current_frame).as_sampled(command_buffer);
		shadow_depth_image->get_image(current_frame).as_sampled(command_buffer);
		sky_cubemap.get_image().as_sampled(command_buffer);
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

	void record_wireframe_pass(const CommandBuffer &command_buffer) {
		depth_image->get_image(current_frame).as_depth_target(command_buffer);
		offscreen_framebuffer->get_image(current_frame).as_color_target(command_buffer);

		std::span screen_pass_attachments = wireframe_attachment_layout.get_color_infos({
			{AttachmentLayout::Type::Color0, &offscreen_framebuffer->get_image_view(current_frame)},
		});
		vk::RenderingInfo light_pass_render_info {
			.renderArea = {.offset = {0, 0}, .extent = swapchain.get_extent()},
			.layerCount = 1,
			.colorAttachmentCount = static_cast<uint32_t>(screen_pass_attachments.size()),
			.pColorAttachments = screen_pass_attachments.data(),
			.pDepthAttachment = &wireframe_attachment_layout.get_depth_info(
				depth_image->get_image_view(current_frame)
			),
		};
		command_buffer.get_command_buffer().beginRendering(light_pass_render_info);
		command_buffer.get_command_buffer().bindPipeline(
			vk::PipelineBindPoint::eGraphics,
			wireframe_pipeline->get_pipeline()
		);
		command_buffer.get_command_buffer().bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			wireframe_pipeline->get_layout(),
			0,
			{
				*camera_descriptor_sets[current_frame].get_descriptor_set(),
				*object_descriptor_sets[current_frame].get_descriptor_set(),
			},
			nullptr
		);

		record_draw_objects(command_buffer, wireframe_selected_only);

		command_buffer.get_command_buffer().endRendering();
	}

	void record_shadow_pass(const CommandBuffer &command_buffer) {
		shadow_depth_image->get_image(current_frame).as_depth_target(command_buffer);
		command_buffer.get_command_buffer().setViewport(
			0,
			vk::Viewport(0.0f, 0.0f, SHADOW_RESOLUTION, SHADOW_RESOLUTION, 0.0f, 1.0f)
		);
		command_buffer.get_command_buffer().setScissor(
			0,
			vk::Rect2D(
				vk::Offset2D(0, 0),
				{
					static_cast<uint32_t>(SHADOW_RESOLUTION),
					static_cast<uint32_t>(SHADOW_RESOLUTION),
				}
			)
		);

		vk::RenderingInfo rendering_info {
			.renderArea =
				{
					.offset = {0, 0},
					.extent =
						{
							static_cast<uint32_t>(SHADOW_RESOLUTION),
							static_cast<uint32_t>(SHADOW_RESOLUTION),
						},
				},
			.layerCount = 1,
			.pDepthAttachment = &shadow_attachment_layout.get_depth_info(
				shadow_depth_image->get_image_view(current_frame)
			),
		};

		command_buffer.get_command_buffer().beginRendering(rendering_info);

		command_buffer.get_command_buffer().bindPipeline(
			vk::PipelineBindPoint::eGraphics,
			shadow_pipeline->get_pipeline()
		);

		command_buffer.get_command_buffer().bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			shadow_pipeline->get_layout(),
			0,
			{
				*camera_descriptor_sets[current_frame].get_descriptor_set(),
				*object_descriptor_sets[current_frame].get_descriptor_set(),
			},
			nullptr
		);

		record_draw_objects(command_buffer, false);

		command_buffer.get_command_buffer().endRendering();

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
		record_shadow_pass(command_buffer);
		record_shading_pass(command_buffer);
		if (skybox_enabled) {
			record_skybox_pass(command_buffer);
		}
		if (wireframe_enabled) {
			record_wireframe_pass(command_buffer);
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

		device.reset_fence(current_frame);
		command_buffers[current_frame].get_command_buffer().reset();

		update_uniform_buffer(current_frame, delta);
		record_command_buffer(image_index.value());

		device.submit_graphics(command_buffers[current_frame], current_frame);

		auto present_result = device.present(swapchain, image_index.value(), current_frame);
		if (!present_result || window.resized) {
			window.resized = false;
			recreate_swapchain();
		}

		current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void update_uniform_buffer(uint32_t current_image, [[maybe_unused]] double delta) {
		glm::mat4 projection_view = camera.get_projection() * camera.get_view();
		glm::vec2 half_yfov_tan = camera.get_half_fov_tan();

		glm::mat4 shadow_projection_view = shadow_camera.get_projection() *
										   shadow_camera.get_view();

		CameraUniformBufferObject ubo {
			.projection_view = projection_view,
			.inverse_projection_view = glm::inverse(projection_view),

			.camera_position = camera.get_position(),
			.camera_forward = camera.get_forward(),
			.camera_right = camera.get_right() * half_yfov_tan.x,
			.camera_up = camera.get_up() * half_yfov_tan.y,
			.z_near = camera.get_z_near(),
			.z_far = camera.get_z_far(),

			.world_light_direction = shadow_camera.get_forward(),
			.shadow_projection_view = shadow_projection_view,

			.debug_options = debug_options,
		};

		uniform_buffers[current_image].upload_data(&ubo);

		ObjectUniformBufferObject object_ubo {};
		for (size_t i = 0; i < models.size(); ++i) {
			auto &model = models[i];
			glm::mat4 transform_matrix = model.get_transform_matrix();
			object_ubo.transform_matrices[model.get_id()] = {
				.transform = transform_matrix,
				.normal = glm::transpose(glm::inverse(transform_matrix)),
				.metalness = model.get_metalness(),
			};
		}

		object_uniform_buffers[current_frame].upload_data(&object_ubo);
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
		std::array filenames = {
			"textures/skybox/px.png",
			"textures/skybox/nx.png",
			"textures/skybox/py.png",
			"textures/skybox/ny.png",
			"textures/skybox/pz.png",
			"textures/skybox/nz.png",
		};
		for (size_t i = 0; i < 6; ++i) {
			auto result = skybox[i].load_from_file(filenames[i]);
			if (!result) {
				throw std::runtime_error(result.error());
			}
		}

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

		sky_cubemap.get_image().as_transfer_dst(cmd);

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

	// TODO: Cleanup code
	[[nodiscard]] UniquePtr<ImageViewPair<Image>> load_image(
		const char *filepath,
		bool is_normal_map
	) {
		ImageData image_data;
		auto result = image_data.load_from_file(filepath);
		if (!result) {
			throw std::runtime_error(result.error());
		}
		Format format = Format::RGBA8Srgb;
		if (is_normal_map) {
			format = Format::RGBA8Unorm;
		}

		UniquePtr<ImageViewPair<Image>> image_pair = std::make_unique<ImageViewPair<Image>>(
			physical_device,
			device,
			image_data.get_width(),
			image_data.get_height(),
			format,
			Image::Usage::Texture,
			true
		);

		StagingBuffer stage(device, image_data.get_size());
		stage.map();
		stage.upload_data(image_data.get_data());
		stage.unmap();

		CommandBuffer cmd = device.allocate_command_buffer();
		cmd.begin_onetime();

		image_pair->get_image().as_transfer_dst(cmd);

		vk::BufferImageCopy2 region {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource =
				{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			.imageOffset = {0, 0, 0},
			.imageExtent = {
				.width = static_cast<uint32_t>(image_data.get_width()),
				.height = static_cast<uint32_t>(image_data.get_height()),
				.depth = 1,
			},
		};

		cmd.get_command_buffer().copyBufferToImage2({
			.srcBuffer = stage.get_buffer(),
			.dstImage = image_pair->get_image().get_image(),
			.dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
			.regionCount = 1,
			.pRegions = &region,
		});

		image_pair->get_image().generate_mipmap(cmd);

		cmd.end();
		device.submit_graphics_onetime(cmd);

		return image_pair;
	}

	void load_heightmap() {
		ImageData image_data;
		auto result = image_data.load_heightmap_from_file("textures/heightmap.png");
		if (!result) {
			throw std::runtime_error(result.error());
		}

		Heightmap heightmap(
			image_data.get_width(),
			std::span {static_cast<const float *>(image_data.get_data()), image_data.get_size()}
		);

		terrain_mesh = std::make_unique<HeightmapTerrainMesh>(200, 200, 128, 128, 10, heightmap);

		terrain_texture = load_image("textures/ground/forrest_ground_01_diff_4k.png", false);
		terrain_normal_texture =
			load_image("textures/ground/forrest_ground_01_nor_gl_4k.png", true);

		debug_log("All terrain textures loaded");
	}

	void load_obj_model() {
		bunny_mesh.load_from_obj_file("models/bunny.obj");
		dragon_mesh.load_from_obj_file("models/dragon.obj");
		lucy_mesh.load_from_obj_file("models/lucy-centered.obj");
	}

	void create_models() {
		assert(terrain_mesh.get() != nullptr);
		uint32_t id = 0;
		Model terrain(id++, static_cast<Mesh *>(terrain_mesh.get()));
		Model cube(id++, static_cast<Mesh *>(&cube_mesh));
		Model bunny(id++, static_cast<Mesh *>(&bunny_mesh));
		Model dragon(id++, static_cast<Mesh *>(&dragon_mesh));
		Model lucy(id++, static_cast<Mesh *>(&lucy_mesh));

		terrain.set_name("Terrain");
		cube.set_name("Cube");
		cube.set_position({0, 1, 0});
		bunny.set_name("Stanford Bunny");
		bunny.set_position({0, 1.5, 3});
		bunny.set_scale(glm::vec3(20));
		dragon.set_name("Stanford Dragon");
		dragon.set_position({10, 0, 0});
		dragon.set_scale(glm::vec3(0.5));
		lucy.set_name("Stanford Lucy");
		lucy.set_position({-10, 0, 0});
		lucy.set_scale(glm::vec3(20));

		models.push_back(terrain);
		models.push_back(cube);
		models.push_back(bunny);
		models.push_back(dragon);
		models.push_back(lucy);

		for (const auto &model : models) {
			id_cache.insert(model.get_id());
		}
		debug_log("All models pushed");
	}

	void copy_object_image() {
		CommandBuffer cmd = device.allocate_command_buffer();
		cmd.begin_onetime();

		object_id_image->get_image(current_frame).as_transfer_src(cmd);

		vk::BufferImageCopy2 region {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource =
				{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			.imageOffset = {0, 0, 0},
			.imageExtent = {
				.width = static_cast<uint32_t>(swapchain.get_extent().width),
				.height = static_cast<uint32_t>(swapchain.get_extent().height),
				.depth = 1,
			},
		};

		cmd.get_command_buffer().copyImageToBuffer2({
			.srcImage = object_id_image->get_image(current_frame).get_image(),
			.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
			.dstBuffer = object_id_staging->get_buffer(),
			.regionCount = 1,
			.pRegions = &region,
		});

		cmd.end();
		device.submit_graphics_onetime(cmd);
	}
};

int main() {
	debug_log("Running in DEBUG mode");

	try {
		ProjectSkyHigh skyhigh;
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
