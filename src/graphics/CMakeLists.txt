add_library(
	${PROJECT_NAME}-graphics
	SHARED
		allocator.cpp
		buffer.cpp
		command_buffer.cpp
		device.cpp
		dispatch_loader.cpp
		instance.cpp
		physical_device.cpp
		surface.cpp
)

target_link_libraries(
	${PROJECT_NAME}-graphics
	PRIVATE
		Vulkan::Headers

		${PROJECT_NAME}-core
)

target_compile_definitions(
	${PROJECT_NAME}-graphics
	PRIVATE
		VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
		VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)
