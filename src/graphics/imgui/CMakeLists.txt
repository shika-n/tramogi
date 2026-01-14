add_library(
	${PROJECT_NAME}-graphics-imgui
	STATIC
		imgui_loader.cpp
)

target_link_libraries(
	${PROJECT_NAME}-graphics-imgui
	PUBLIC
		imgui
		vulkan
)
