set(TINYGLTF_DIR ${TINYRHI_EXAMPLES_ROOT}/dependences/tinygltf)

tinyrhi_add_example(tinyrhi_ibl_pbr ibl_pbr.cpp)

target_include_directories(tinyrhi_ibl_pbr
    PRIVATE
        ${TINYGLTF_DIR}
)

target_link_libraries(tinyrhi_ibl_pbr PRIVATE stb::stb)

add_custom_command(TARGET tinyrhi_ibl_pbr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/plains_sunset_2k.hdr
        $<TARGET_FILE_DIR:tinyrhi_ibl_pbr>/plains_sunset_2k.hdr
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/glTF
        $<TARGET_FILE_DIR:tinyrhi_ibl_pbr>/glTF
)
