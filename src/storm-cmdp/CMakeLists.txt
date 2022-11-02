file(GLOB_RECURSE ALL_FILES ${PROJECT_SOURCE_DIR}/src/storm-cmdp/*.h ${PROJECT_SOURCE_DIR}/src/storm-cmdp/*.cpp)

register_source_groups_from_filestructure("${ALL_FILES}" storm-cmdp)



file(GLOB_RECURSE STORM_CMDP_SOURCES ${PROJECT_SOURCE_DIR}/src/storm-cmdp/*/*.cpp)
file(GLOB_RECURSE STORM_CMDP_HEADERS ${PROJECT_SOURCE_DIR}/src/storm-cmdp/*/*.h)


# Create storm-cmdp.
add_library(storm-cmdp SHARED ${STORM_CMDP_SOURCES} ${STORM_CMDP_HEADERS})

# Remove define symbol for shared libstorm.
set_target_properties(storm-cmdp PROPERTIES DEFINE_SYMBOL "")
#add_dependencies(storm resources)
list(APPEND STORM_TARGETS storm-cmdp)
set(STORM_TARGETS ${STORM_TARGETS} PARENT_SCOPE)

target_link_libraries(storm-cmdp PUBLIC storm ${STORM_CMDP_LINK_LIBRARIES})

# Install storm headers to include directory.
foreach(HEADER ${STORM_CMDP_HEADERS})
	string(REGEX REPLACE "${PROJECT_SOURCE_DIR}/src/?" "" RELATIVE_HEADER_PATH ${HEADER})
	string(REGEX MATCH "(.*)[/\\]" RELATIVE_DIRECTORY ${RELATIVE_HEADER_PATH})
	string(REGEX REPLACE "${RELATIVE_DIRECTORY}/?" "" HEADER_FILENAME ${RELATIVE_HEADER_PATH})
	add_custom_command(
		OUTPUT ${CMAKE_BINARY_DIR}/include/${RELATIVE_DIRECTORY}${HEADER_FILENAME}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include/${RELATIVE_DIRECTORY}
		COMMAND ${CMAKE_COMMAND} -E copy ${HEADER} ${CMAKE_BINARY_DIR}/include/${RELATIVE_DIRECTORY}${HEADER_FILENAME}
		DEPENDS ${HEADER}
	)
    list(APPEND STORM_CMDP_OUTPUT_HEADERS "${CMAKE_BINARY_DIR}/include/${RELATIVE_DIRECTORY}${HEADER_FILENAME}")
endforeach()
add_custom_target(copy_storm_cmdp_headers DEPENDS ${STORM_CMDP_OUTPUT_HEADERS} ${STORM_CMDP_HEADERS})
add_dependencies(storm-cmdp copy_storm_cmdp_headers)

# installation
install(TARGETS storm-cmdp EXPORT storm_Targets RUNTIME DESTINATION bin LIBRARY DESTINATION lib OPTIONAL)

