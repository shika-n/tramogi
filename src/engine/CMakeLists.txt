add_library(
	${PROJECT_NAME}-engine
	SHARED
		camera.cpp
		ortho_camera.cpp

		primitives/cube.cpp
		primitives/heightmap_terrain.cpp
)

target_link_libraries(
	${PROJECT_NAME}-engine
	PRIVATE
		glm

		${PROJECT_NAME}-core
)
