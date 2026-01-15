add_library(
	${PROJECT_NAME}-engine
	SHARED
		camera.cpp
)

target_link_libraries(
	${PROJECT_NAME}-engine
	PRIVATE
		glm

		${PROJECT_NAME}-core
)
