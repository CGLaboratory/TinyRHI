tinyrhi_add_example(tinyrhi_textured_quad textured_quad.cpp)

add_custom_command(TARGET tinyrhi_textured_quad POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/test.png
        $<TARGET_FILE_DIR:tinyrhi_textured_quad>/test.png
)
