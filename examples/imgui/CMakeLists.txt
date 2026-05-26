set(IMGUI_DIR ${TINYRHI_EXAMPLES_ROOT}/dependences/imgui)

tinyrhi_add_example(tinyrhi_imgui
    main.cpp
    imgui_viewport_surface.cpp
    imgui_sample_app.cpp
    tiny_rhi_imgui_renderer.cpp
    win32_imgui_platform.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

target_include_directories(tinyrhi_imgui
    PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

target_link_libraries(tinyrhi_imgui PRIVATE gdi32 dwmapi)
