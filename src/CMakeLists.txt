add_executable(
	${PROJECT_NAME}
	main.cpp
)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		-static-libstdc++
		stdc++exp
		glfw
		glm::glm
		Vulkan::Headers
)

