add_subdirectory(imgui)

add_library(
	${PROJECT_NAME}-graphics
	SHARED
		allocator.cpp
		buffer.cpp
		command_buffer.cpp
		device.cpp
		dispatch_loader.cpp
		image.cpp
		image_view.cpp
		instance.cpp
		physical_device.cpp
		surface.cpp
)

target_link_libraries(
	${PROJECT_NAME}-graphics
	PRIVATE
		vulkan

		${PROJECT_NAME}-core
)
