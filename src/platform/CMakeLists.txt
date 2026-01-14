add_library(
	${PROJECT_NAME}-platform
	SHARED
	glfw_window.cpp
)

target_link_libraries(
	${PROJECT_NAME}-platform
	PRIVATE
		glfw
		vulkan

		${PROJECT_NAME}-core
)
