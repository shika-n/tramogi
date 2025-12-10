add_library(
	${PROJECT_NAME}-core-file
	SHARED
		stb_wrapper.cpp
		tinyobj_wrapper.cpp
)

target_link_libraries(
	${PROJECT_NAME}-core-file
	PRIVATE
		stdc++exp
		tinyobjloader
)

target_include_directories(
	${PROJECT_NAME}-core-file
	PRIVATE
		${PROJECT_SOURCE_DIR}/external/stb
)
