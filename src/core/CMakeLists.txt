add_library(
	${PROJECT_NAME}-core
	SHARED
)

target_link_libraries(
	${PROJECT_NAME}-core
	PRIVATE
		stdc++exp
)

add_subdirectory(io)
add_subdirectory(logging)
