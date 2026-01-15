#pragma once

struct ImGuiContext;
struct ImGui_ImplVulkan_InitInfo;

namespace vk {
class CommandBuffer;
}

namespace tramogi {

namespace platform {
class Window;
}

namespace graphics::imgui {

ImGuiContext *init(const platform::Window &window, ImGui_ImplVulkan_InitInfo *imgui_init_info);
void next_frame();
void end_frame();
void render(vk::CommandBuffer cmd);
void cleanup();

} // namespace graphics::imgui
} // namespace tramogi
