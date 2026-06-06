add_executable(tinyrhi_tests
    backend_factory_tests.cpp
    bind_group_tests.cpp
    gl_convert_tests.cpp
    main.cpp
    pipeline_layout_tests.cpp
    rhi_type_tests.cpp
    texture_array_tests.cpp
)

target_link_libraries(tinyrhi_tests
    PRIVATE
        TinyRHI::TinyRHI
)

target_include_directories(tinyrhi_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(tinyrhi_tests PRIVATE cxx_std_20)

add_test(NAME tinyrhi_tests COMMAND tinyrhi_tests)
