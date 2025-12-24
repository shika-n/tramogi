add_subdirectory(core)
add_subdirectory(graphics)
add_subdirectory(input)
add_subdirectory(platform)

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
		glm::glm
		Vulkan::Headers

		${PROJECT_NAME}-core
		${PROJECT_NAME}-core-file
		${PROJECT_NAME}-graphics
		${PROJECT_NAME}-input
		${PROJECT_NAME}-platform
)

target_compile_definitions(
	${PROJECT_NAME}
	PRIVATE
		VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
		VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
		GLM_FORCE_DEPTH_ZERO_TO_ONE=1
		GLM_ENABLE_EXPERIMENTAL=1
)

function (add_slang_shader_target TARGET)
	cmake_parse_arguments("SHADER" "" "" "SOURCES" ${ARGN})
	set(SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
	set(SHADERS_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/shaders)
	set(SHADER_SOURCES ${SHADERS_DIR}/shader.slang)
	set(ENTRY_POINTS -entry vert_main -entry frag_main)
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
		OUTPUT ${SHADERS_OUTPUT_DIR}/slang.spv
		COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES}
			-target spirv
			-profile spirv_1_4
			-emit-spirv-directly
			-fvk-use-entrypoint-name ${ENTRY_POINTS}
			-o ${SHADERS_OUTPUT_DIR}/slang.spv
		WORKING_DIRECTORY ${SHADERS_DIR}
		DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
		COMMENT "Compiling Slang shaders"
		VERBATIM
	)
	add_custom_target(${TARGET} DEPENDS ${SHADERS_OUTPUT_DIR}/slang.spv)
endfunction()

add_slang_shader_target(slang SOURCES ${SHADER_SLANG_SOURCES})
add_dependencies(${PROJECT_NAME} slang)
