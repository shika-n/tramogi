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
