#include "tramogi/graphics/imgui/imgui_loader.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>

#include "tramogi/platform/window.h"

namespace tramogi::graphics::imgui {

using platform::Window;

ImGuiContext *init(const Window &window, ImGui_ImplVulkan_InitInfo *imgui_init_info) {
	IMGUI_CHECKVERSION();
	ImGuiContext *ctx = ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui_ImplVulkan_LoadFunctions(
		vk::ApiVersion14,
		[](const char *function_name, void *user_data) -> PFN_vkVoidFunction {
			VkInstance instance = static_cast<VkInstance>(user_data);
			return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(instance, function_name);
		},
		imgui_init_info->Instance
	);

	ImGui_ImplGlfw_InitForVulkan(window.get_glfw_window(), true);
	ImGui_ImplVulkan_Init(imgui_init_info);

	return ctx;
}

void next_frame() {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplVulkan_NewFrame();
	ImGui::NewFrame();

	ImGuiID dockspace_id = ImGui::GetID("Main Dockspace");
	ImGuiViewport *viewport = ImGui::GetMainViewport();
	if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
		ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

		ImGuiID left_split_id;
		ImGuiID center_split_id;
		ImGui::DockBuilderSplitNode(
			dockspace_id,
			ImGuiDir_Left,
			0.2f,
			&left_split_id,
			&center_split_id
		);

		ImGuiID right_split_id;
		ImGuiID center2_split_id;
		ImGui::DockBuilderSplitNode(
			dockspace_id,
			ImGuiDir_Right,
			0.2f,
			&right_split_id,
			&center2_split_id
		);

		ImGuiID upper_right_split_id;
		ImGuiID bottom_right_split_id;
		ImGui::DockBuilderSplitNode(
			right_split_id,
			ImGuiDir_Up,
			0.4f,
			&upper_right_split_id,
			&bottom_right_split_id
		);

		ImGui::DockBuilderDockWindow("Properties", left_split_id);
		ImGui::DockBuilderDockWindow("Objects", upper_right_split_id);
		ImGui::DockBuilderDockWindow("Object Property", bottom_right_split_id);
	}

	ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
}

void end_frame() {
	ImGui::Render();
}

void render(vk::CommandBuffer cmd) {
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void cleanup() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

} // namespace tramogi::graphics::imgui
