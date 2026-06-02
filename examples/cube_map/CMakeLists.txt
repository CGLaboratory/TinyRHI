tinyrhi_add_example(tinyrhi_cube_map cube_map.cpp)

target_link_libraries(tinyrhi_cube_map PRIVATE stb::stb)

add_custom_command(TARGET tinyrhi_cube_map POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/newport_loft.hdr
        $<TARGET_FILE_DIR:tinyrhi_cube_map>/newport_loft.hdr
)
