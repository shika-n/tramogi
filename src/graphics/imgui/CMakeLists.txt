add_library(
	${PROJECT_NAME}-graphics-imgui
	SHARED
		imgui_loader.cpp
)

target_link_libraries(
	${PROJECT_NAME}-graphics-imgui
	PUBLIC
		imgui
)

target_compile_definitions(
	${PROJECT_NAME}-graphics-imgui
	PRIVATE
		VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
		VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)
