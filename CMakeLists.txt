cmake_minimum_required(VERSION 3.31)

project("SkyHigh" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(VulkanHeaders REQUIRED)


add_executable(
	${PROJECT_NAME}
	src/main.cpp
)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		-static-libstdc++
		stdc++exp
		glfw glm::glm
		Vulkan::Headers
)
