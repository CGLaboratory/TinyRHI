#include "imgui_viewport_surface.h"

namespace tinyrhi_examples {

ImGuiViewportSurface::ImGuiViewportSurface(void* window, uint32_t width, uint32_t height)
    : m_window(window),
      m_width(width),
      m_height(height)
{}

lunalite::rhi::NativeSurfaceHandle ImGuiViewportSurface::getNativeHandle() const
{
    return lunalite::rhi::NativeSurfaceHandle{
        .platform = lunalite::rhi::NativeSurfaceHandle::Platform::Win32,
        .display = nullptr,
        .window = m_window,
    };
}

uint32_t ImGuiViewportSurface::getWidth() const
{
    return m_width;
}

uint32_t ImGuiViewportSurface::getHeight() const
{
    return m_height;
}

void ImGuiViewportSurface::resize(uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;
}

} // namespace tinyrhi_examples
