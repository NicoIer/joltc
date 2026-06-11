if (NOT JPH_SAMPLES)
    return ()
endif ()

function(add_sample SAMPLE_NAME)
    file(GLOB SOURCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/${SAMPLE_NAME}/*.h"
	    "${CMAKE_CURRENT_SOURCE_DIR}/${SAMPLE_NAME}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/${SAMPLE_NAME}/*.cpp"
    )

    if (ANDROID)
        add_library(${SAMPLE_NAME} SHARED ${SOURCE_FILES})
    else ()
    	add_executable(${SAMPLE_NAME} ${SOURCE_FILES})
    endif ()

    if (DOUBLE_PRECISION)
        target_link_libraries(${SAMPLE_NAME} LINK_PUBLIC joltc_double)
    else()
        target_link_libraries(${SAMPLE_NAME} LINK_PUBLIC joltc)
    endif ()

	if (MSVC)
		target_link_options(${SAMPLE_NAME} PUBLIC "/SUBSYSTEM:CONSOLE")
	endif()

    set_target_properties(${SAMPLE_NAME} PROPERTIES 
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${SAMPLE_NAME}>"
        FOLDER "samples"
    )

	if (JPH_VIEWER_GRAPHICS_AVAILABLE AND SAMPLE_NAME STREQUAL "jolt_viewer_c")
		add_custom_command(TARGET ${SAMPLE_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory
				"${JOLTC_JOLT_PHYSICS_ROOT}/Assets/Fonts"
				"$<TARGET_FILE_DIR:${SAMPLE_NAME}>/Fonts"
			COMMAND ${CMAKE_COMMAND} -E copy_directory
				"${JOLTC_JOLT_PHYSICS_ROOT}/Assets/Shaders"
				"$<TARGET_FILE_DIR:${SAMPLE_NAME}>/Shaders"
		)
		if (JOLTC_VIEWER_METAL_LIB)
			add_custom_command(TARGET ${SAMPLE_NAME} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
					"${JOLTC_VIEWER_METAL_LIB}"
					"$<TARGET_FILE_DIR:${SAMPLE_NAME}>/Shaders/MTL/Shaders.metallib"
			)
		endif()
	endif()
endfunction()

# Add samples
add_sample(01_HelloWorld)
add_sample(02_RemoveBodyOnContact)
add_sample(jolt_viewer_c)
