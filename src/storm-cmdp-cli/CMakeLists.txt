# Create storm-cmdp.
add_executable(storm-cmdp-cli ${PROJECT_SOURCE_DIR}/src/storm-cmdp-cli/storm-cmdp.cpp)
target_link_libraries(storm-cmdp-cli storm-cmdp storm-cli-utilities) # Adding headers for xcode
set_target_properties(storm-cmdp-cli PROPERTIES OUTPUT_NAME "storm-cmdp")

add_dependencies(binaries storm-cmdp-cli)

# installation
install(TARGETS storm-cmdp-cli EXPORT storm_Targets RUNTIME DESTINATION bin LIBRARY DESTINATION lib OPTIONAL)
