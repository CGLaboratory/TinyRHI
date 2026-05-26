#include "swapchain.h"

#include "device.h"

#include <memory>

namespace lunalite::rhi {

SwapchainHandle OpenGLDevice::createSwapchain(Surface& surface, const SwapchainDesc& desc)
{
    SwapchainHandle handle = 0;
    for (size_t i = 0; i < m_swapchains.size(); ++i) {
        if (m_swapchains[i] == nullptr) {
            handle = static_cast<SwapchainHandle>(i + 1);
            break;
        }
    }

    if (handle == 0) {
        handle = static_cast<SwapchainHandle>(m_swapchains.size() + 1);
    }

    auto swapchain = std::make_unique<OpenGLSwapchain>(*this, handle, surface, desc);
    if (!swapchain->initialize()) {
        return 0;
    }

    if (handle > m_swapchains.size()) {
        m_swapchains.push_back(std::move(swapchain));
    } else {
        m_swapchains[handle - 1] = std::move(swapchain);
    }

    return handle;
}

void OpenGLDevice::destroySwapchain(SwapchainHandle swapchain)
{
    if (getOpenGLSwapchain(swapchain) == nullptr) {
        return;
    }

    m_swapchains[swapchain - 1].reset();
}

Swapchain* OpenGLDevice::getSwapchain(SwapchainHandle swapchain)
{
    return getOpenGLSwapchain(swapchain);
}

OpenGLSwapchain* OpenGLDevice::getOpenGLSwapchain(SwapchainHandle handle)
{
    if (handle == 0 || handle > m_swapchains.size()) {
        return nullptr;
    }

    return m_swapchains[handle - 1].get();
}

bool OpenGLDevice::ensureContextForSwapchain(OpenGLSwapchain& swapchain, bool vsync)
{
    return ensureOpenGLNativeContext(m_native_context, swapchain.nativeSwapchain(), vsync);
}

bool OpenGLDevice::makeSwapchainCurrent(SwapchainHandle swapchain)
{
    auto* glSwapchain = getOpenGLSwapchain(swapchain);
    if (glSwapchain == nullptr) {
        return false;
    }

    return makeOpenGLNativeContextCurrent(m_native_context, glSwapchain->nativeSwapchain());
}

bool OpenGLDevice::makeAnySwapchainCurrent()
{
    for (size_t i = 0; i < m_swapchains.size(); ++i) {
        if (m_swapchains[i] != nullptr && makeSwapchainCurrent(static_cast<SwapchainHandle>(i + 1))) {
            return true;
        }
    }

    return false;
}

void OpenGLDevice::releaseContext()
{
    destroyOpenGLNativeContext(m_native_context);
}

void OpenGLDevice::releaseNativeSwapchain(OpenGLNativeSwapchain& swapchain)
{
    destroyOpenGLNativeSwapchain(m_native_context, swapchain);
}

OpenGLSwapchain::OpenGLSwapchain(OpenGLDevice& device,
                                 SwapchainHandle handle,
                                 Surface& surface,
                                 const SwapchainDesc& desc)
    : m_device(device),
      m_handle(handle),
      m_surface(surface),
      m_desc(desc)
{}

OpenGLSwapchain::~OpenGLSwapchain()
{
    if (m_color_view != 0) {
        m_device.destroyTextureView(m_color_view);
        m_color_view = 0;
    }

    if (m_depth_stencil_view != 0) {
        m_device.destroyTextureView(m_depth_stencil_view);
        m_depth_stencil_view = 0;
    }

    m_device.releaseNativeSwapchain(m_native);
}

bool OpenGLSwapchain::initialize()
{
    if (!createOpenGLNativeSwapchain(m_surface.getNativeHandle(), m_native)) {
        return false;
    }

    if (!m_device.ensureContextForSwapchain(*this, m_desc.vsync)) {
        return false;
    }

    m_color_view = m_device.createSwapchainTextureView(m_desc.color_format, m_handle);
    if (m_desc.enable_depth_stencil) {
        m_depth_stencil_view = m_device.createSwapchainTextureView(m_desc.depth_stencil_format, m_handle);
    }

    resize(m_surface.getWidth(), m_surface.getHeight());
    return true;
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
    m_device.resizeSwapchainTextureView(m_color_view, width, height);
    m_device.resizeSwapchainTextureView(m_depth_stencil_view, width, height);
}

void OpenGLSwapchain::present()
{
    if (!m_device.makeSwapchainCurrent(m_handle)) {
        return;
    }

    presentOpenGLNativeSwapchain(m_native);
}

SwapchainHandle OpenGLSwapchain::handle() const
{
    return m_handle;
}

OpenGLNativeSwapchain& OpenGLSwapchain::nativeSwapchain()
{
    return m_native;
}

} // namespace lunalite::rhi
