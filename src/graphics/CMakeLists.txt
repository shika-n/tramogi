add_subdirectory(imgui)

add_library(
	${PROJECT_NAME}-graphics
	SHARED
		allocator.cpp
		attachment_info.cpp
		buffer.cpp
		command_buffer.cpp
		descriptor_layout.cpp
		descriptor_pool.cpp
		descriptor_set.cpp
		device.cpp
		dispatch_loader.cpp
		format.cpp
		image.cpp
		image_view.cpp
		instance.cpp
		physical_device.cpp
		pipeline.cpp
		shader.cpp
		surface.cpp
		swapchain.cpp
		vertex_descriptor.cpp
)

target_link_libraries(
	${PROJECT_NAME}-graphics
	PRIVATE
		vulkan

		${PROJECT_NAME}-core
)
