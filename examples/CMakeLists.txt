if(NOT WIN32)
    message(WARNING "TinyRHI examples are skipped because the current helper is Win32/WGL-only.")
    return()
endif()

function(tinyrhi_add_example target)
    add_executable(${target}
        ${ARGN}
        common/win32_gl_surface.cpp
    )

    target_link_libraries(${target}
        PRIVATE
            TinyRHI::TinyRHI
    )

    target_include_directories(${target}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_features(${target} PRIVATE cxx_std_20)

    target_link_libraries(${target} PRIVATE opengl32)
endfunction()

tinyrhi_add_example(tinyrhi_clear_window clear_window.cpp)
tinyrhi_add_example(tinyrhi_triangle triangle.cpp)
tinyrhi_add_example(tinyrhi_indexed_quad indexed_quad.cpp)
tinyrhi_add_example(tinyrhi_uniform_triangle uniform_triangle.cpp)
tinyrhi_add_example(tinyrhi_textured_quad textured_quad.cpp)
tinyrhi_add_example(tinyrhi_depth_test depth_test.cpp)
