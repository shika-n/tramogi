add_library(
	${PROJECT_NAME}-core-file
	SHARED
		file.cpp
		model.cpp
		stb_wrapper.cpp
)

target_link_libraries(
	${PROJECT_NAME}-core-file
	PRIVATE
		glm::glm
		stdc++exp
		tinyobjloader
)

target_include_directories(
	${PROJECT_NAME}-core-file
	PRIVATE
		${PROJECT_SOURCE_DIR}/external/stb
)
