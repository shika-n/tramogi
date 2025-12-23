add_library(
	${PROJECT_NAME}-input
	SHARED
		keyboard.cpp
)

target_link_libraries(
	${PROJECT_NAME}-input
	PRIVATE
		stdc++exp
)
