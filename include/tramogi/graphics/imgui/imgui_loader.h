#pragma once

namespace vk {
class CommandBuffer;
}
struct ImGui_ImplVulkan_InitInfo;

namespace tramogi {

namespace platform {
class Window;
}

namespace graphics::imgui {

void init(const platform::Window &window, ImGui_ImplVulkan_InitInfo *imgui_init_info);
void next_frame();
void end_frame();
void render(vk::CommandBuffer cmd);
void cleanup();

} // namespace graphics::imgui
} // namespace tramogi
