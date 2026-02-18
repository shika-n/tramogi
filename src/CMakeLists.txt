add_subdirectory(core)
add_subdirectory(engine)
add_subdirectory(graphics)
add_subdirectory(input)
add_subdirectory(platform)
add_subdirectory(renderer)

add_executable(
	${PROJECT_NAME}
	main.cpp
)

set_target_properties(
	${PROJECT_NAME}
	PROPERTIES
		OUTPUT_NAME Tramogi
)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		glm
		vulkan

		${PROJECT_NAME}-core
		${PROJECT_NAME}-core-file
		${PROJECT_NAME}-engine
		${PROJECT_NAME}-graphics
		${PROJECT_NAME}-graphics-imgui
		${PROJECT_NAME}-input
		${PROJECT_NAME}-platform
		${PROJECT_NAME}-renderer
)

function (add_slang_shader_target TARGET)
	cmake_parse_arguments(PARSE_ARGV 0 "SHADER" "" "OUTPUT" "SOURCES;ENTRY_POINTS")
	set(SHADERS_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/shaders)

	message(STATUS "Shader target!")
	message(STATUS "OUTPUT: ${SHADER_OUTPUT}")
	message(STATUS "SOURCES: ${SHADER_SOURCES}")
	message(STATUS "ENTRY_POINTS: ${SHADER_ENTRY_POINTS}")

	add_custom_command(
		OUTPUT ${SHADERS_DIR}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
	)
	add_custom_command(
		OUTPUT ${SHADERS_OUTPUT_DIR}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_OUTPUT_DIR}
	)

	if (NOT DEFINED SLANGC_EXECUTABLE)
		message(FATAL_ERROR "No slangc defined. Try running build.py build --generate")
	endif()

	add_custom_command(
		OUTPUT ${SHADERS_OUTPUT_DIR}/${SHADER_OUTPUT}
		COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES}
			-target spirv
			-profile spirv_1_4
			-emit-spirv-directly
			-fvk-use-entrypoint-name ${SHADER_ENTRY_POINTS}
			-o ${SHADERS_OUTPUT_DIR}/${SHADER_OUTPUT}
		WORKING_DIRECTORY ${SHADERS_DIR}
		DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
		COMMENT "Compiling Slang shaders"
		VERBATIM
	)
	add_custom_target(${TARGET} DEPENDS ${SHADERS_OUTPUT_DIR}/${SHADER_OUTPUT})
endfunction()

set(SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
add_slang_shader_target(slang-gbuffer
	OUTPUT
		gbuffer.spv
	SOURCES
		${SHADERS_DIR}/gbuffer.slang
	ENTRY_POINTS
		-entry vert_main
		-entry frag_main
		-entry shadow_vert
		-entry basic_vert
		-entry solid_frag
)
add_slang_shader_target(slang-shading
	OUTPUT
		shading.spv
	SOURCES
		${SHADERS_DIR}/shading.slang
	ENTRY_POINTS
		-entry screen_vert
		-entry shading_frag
)
add_slang_shader_target(slang-skybox
	OUTPUT
		skybox.spv
	SOURCES
		${SHADERS_DIR}/skybox.slang
	ENTRY_POINTS
		-entry screen_vert
		-entry skybox_frag
)
add_dependencies(${PROJECT_NAME}
	slang-gbuffer
	slang-shading
	slang-skybox
)
