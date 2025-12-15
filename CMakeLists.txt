cmake_minimum_required(VERSION 3.31)

project("tramogi" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/Vulkan-Headers)
add_subdirectory(external/tinyobjloader)

include_directories(include)

set_target_properties(
	tinyobjloader
	PROPERTIES
		POSITION_INDEPENDENT_CODE ON
)

add_compile_options(
	-pedantic
	-Wall
	-Werror
	-Wextra
)

add_subdirectory(src)
