add_library(
	${PROJECT_NAME}-core-file
	SHARED
		file.cpp
		stb_wrapper.cpp
)

target_link_libraries(
	${PROJECT_NAME}-core-file
	PRIVATE
		glm
		stb
		tinyobj

		${PROJECT_NAME}-core
)
