add_library(
	${PROJECT_NAME}-graphics-imgui
	STATIC
		imgui_loader.cpp
)

target_link_libraries(
	${PROJECT_NAME}-graphics-imgui
	PUBLIC
		imgui_glfw_vulkan
	PRIVATE
		vulkan

		${PROJECT_NAME}-platform
		${PROJECT_NAME}-graphics
)
