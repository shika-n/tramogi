#include "tramogi/graphics/imgui/imgui_loader.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>

#include "tramogi/platform/window.h"

namespace tramogi::graphics::imgui {

using platform::Window;

void init(const Window &window, ImGui_ImplVulkan_InitInfo *imgui_init_info) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

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
}

void next_frame() {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplVulkan_NewFrame();
	ImGui::NewFrame();
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

