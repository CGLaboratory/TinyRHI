add_library(TinyRHI STATIC
    backend_factory.cpp
    opengl/buffer.cpp
    opengl/command_list.cpp
    opengl/device.cpp
    opengl/gl_convert.cpp
    opengl/instance.cpp
    opengl/pipeline.cpp
    opengl/shader.cpp
    opengl/surface.cpp
    opengl/swapchain.cpp
    opengl/timestamp_query.cpp
    opengl/texture.cpp
    opengl/wgl_swapchain_win32.cpp
)

add_library(TinyRHI::TinyRHI ALIAS TinyRHI)

target_include_directories(TinyRHI
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(TinyRHI
    PUBLIC
        glad::glad
)

if(WIN32)
    target_link_libraries(TinyRHI PUBLIC opengl32)
endif()

target_compile_features(TinyRHI PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(TinyRHI PRIVATE /W4 /permissive-)
else()
    target_compile_options(TinyRHI PRIVATE -Wall -Wextra -Wpedantic)
endif()
