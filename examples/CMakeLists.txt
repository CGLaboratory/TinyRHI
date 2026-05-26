if(NOT WIN32)
    message(WARNING "TinyRHI examples are skipped because the current helper is Win32/WGL-only.")
    return()
endif()

set(TINYRHI_EXAMPLES_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

function(tinyrhi_add_example target)
    add_executable(${target}
        ${ARGN}
        ${TINYRHI_EXAMPLES_ROOT}/common/win32_gl_surface.cpp
    )

    target_link_libraries(${target}
        PRIVATE
            TinyRHI::TinyRHI
    )

    target_include_directories(${target}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${TINYRHI_EXAMPLES_ROOT}
    )

    target_compile_features(${target} PRIVATE cxx_std_20)

    target_link_libraries(${target} PRIVATE opengl32)
endfunction()

add_subdirectory(clear_window)
add_subdirectory(triangle)
add_subdirectory(dynamic_uniform_offsets)
add_subdirectory(multi_vertex_buffers)
add_subdirectory(push_constants)
add_subdirectory(textured_quad)
add_subdirectory(depth_test)
add_subdirectory(alpha_blend)
add_subdirectory(line_grid)
add_subdirectory(scissor)
add_subdirectory(imgui)
