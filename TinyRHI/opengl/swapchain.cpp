#include "swapchain.h"

#include "device.h"
#include "../interface/surface.h"

namespace lunalite::rhi {
OpenGLSwapchain::OpenGLSwapchain(OpenGLDevice& device, Surface& surface)
    : m_device(device),
      m_surface(surface),
      m_color_view(m_device.createSwapchainTextureView(TextureFormat::RGBA8)),
      m_depth_stencil_view(m_device.createSwapchainTextureView(TextureFormat::Depth24Stencil8))
{
    uint32_t width = m_surface.getWidth();
    uint32_t height = m_surface.getHeight();
    const auto& gl_surface = m_surface.getSurfaceDesc().opengl;
    if (gl_surface.get_framebuffer_size != nullptr) {
        gl_surface.get_framebuffer_size(gl_surface.user_data, width, height);
    }

    m_surface.resize(width, height);
    resize(width, height);
}

TextureViewHandle OpenGLSwapchain::getCurrentColorTextureView() const
{
    return m_color_view;
}

TextureViewHandle OpenGLSwapchain::getDepthStencilTextureView() const
{
    return m_depth_stencil_view;
}

uint32_t OpenGLSwapchain::getWidth() const
{
    return m_width;
}

uint32_t OpenGLSwapchain::getHeight() const
{
    return m_height;
}

void OpenGLSwapchain::resize(uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;
}

void OpenGLSwapchain::present()
{
    const auto& gl_surface = m_surface.getSurfaceDesc().opengl;
    if (gl_surface.swap_buffers != nullptr) {
        gl_surface.swap_buffers(gl_surface.user_data);
    }
}
} // namespace lunalite::rhi
