cmake_minimum_required(VERSION 3.31)

project("tramogi" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set(CMAKE_SKIP_BUILD_RPATH TRUE)

add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/Vulkan-Headers)
add_subdirectory(external/tinyobjloader)

include_directories(external/stb)
include_directories(include)

add_compile_options(
	-pedantic
	-Wall
	-Werror
	-Wextra
)

add_subdirectory(src)

target_compile_definitions(
	${PROJECT_NAME}
	PUBLIC
		VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
		VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)
