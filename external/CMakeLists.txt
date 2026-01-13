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

target_include_directories(
	imgui
	SYSTEM
	PUBLIC
		imgui
)

target_link_libraries(
	imgui
	PUBLIC
		glfw
		Vulkan::Headers
)

set_target_properties(
	imgui
	PROPERTIES
		POSITION_INDEPENDENT_CODE ON
)

target_compile_definitions(
	imgui
	PRIVATE
		IMGUI_IMPL_VULKAN_NO_PROTOTYPES=1
)
