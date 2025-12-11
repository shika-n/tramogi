add_library(
	${PROJECT_NAME}-graphics
	SHARED
		allocator.cpp
		buffer.cpp
		dispatch_loader.cpp
		#texture.cpp
)

target_link_libraries(
	${PROJECT_NAME}-graphics
	PRIVATE
		Vulkan::Headers
)

target_compile_definitions(
	${PROJECT_NAME}-graphics
	PRIVATE
		VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
		VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)
