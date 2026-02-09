add_library(
	${PROJECT_NAME}-core
	SHARED
	heightmap.cpp
)

target_link_libraries(
	${PROJECT_NAME}-core
	PRIVATE
		stdc++exp
)

add_subdirectory(io)
add_subdirectory(logging)
