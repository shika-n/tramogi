add_library(
	${PROJECT_NAME}-renderer
	SHARED
		primitives/cube.cpp
		primitives/heightmap_terrain.cpp

		mesh.cpp
		model.cpp
)

target_link_libraries(
	${PROJECT_NAME}-renderer
	PRIVATE
		glm
		tinyobj

		${PROJECT_NAME}-core
		${PROJECT_NAME}-graphics
)
