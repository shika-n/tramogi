# GLFW
add_subdirectory(glfw SYSTEM)

# glm
add_library(glm INTERFACE)
target_sources(glm INTERFACE glm/glm/glm.hpp)
target_include_directories(glm SYSTEM INTERFACE glm)
target_compile_definitions(
	glm
	INTERFACE
		GLM_FORCE_DEPTH_ZERO_TO_ONE
		GLM_ENABLE_EXPERIMENTAL
)

# ImGui
add_library(
	imgui
	STATIC
		imgui/imgui.cpp
		imgui/imgui_demo.cpp
		imgui/imgui_draw.cpp
		imgui/imgui_tables.cpp
		imgui/imgui_widgets.cpp
		imgui/backends/imgui_impl_vulkan.cpp
		imgui/backends/imgui_impl_glfw.cpp
)
target_include_directories(imgui SYSTEM PUBLIC imgui)
target_link_libraries(
	imgui
	PRIVATE
		glfw
		vulkan
)
target_compile_definitions(
	imgui
	PRIVATE
		IMGUI_IMPL_VULKAN_NO_PROTOTYPES=1
)
set_target_properties(
	imgui
	PROPERTIES
		POSITION_INDEPENDENT_CODE ON
)

# stb
add_library(stb INTERFACE)
target_sources(stb INTERFACE stb/stb_image.h)
target_include_directories(stb SYSTEM INTERFACE stb)

# tinyobjloader
add_library(tinyobj INTERFACE)
target_sources(tinyobj INTERFACE tinyobjloader/tiny_obj_loader.h tinyobjloader/tiny_obj_loader.cc)
target_include_directories(tinyobj SYSTEM INTERFACE tinyobjloader)
set_target_properties(
	tinyobj
	PROPERTIES
		POSITION_INDEPENDENT_CODE ON
)

# Vulkan
add_library(vulkan INTERFACE)
target_sources(vulkan INTERFACE Vulkan-Headers/include/vulkan/vulkan_raii.hpp)
target_include_directories(vulkan SYSTEM INTERFACE Vulkan-Headers/include)
target_compile_definitions(
	vulkan
	INTERFACE
		VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
		VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
)
