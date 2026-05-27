#include "swapchain.h"

#include "device.h"

#include <memory>

namespace lunalite::rhi {

SwapchainHandle OpenGLDevice::createSwapchain(SurfaceHandle surface, const SwapchainDesc& desc)
{
    if (getSurface(surface) == nullptr) {
        return {};
    }

    SwapchainHandle handle;
    for (size_t i = 0; i < m_swapchains.size(); ++i) {
        if (m_swapchains[i] == nullptr) {
            handle = makeHandle<SwapchainHandle>(i);
            break;
        }
    }

    if (!handle) {
        handle = makeHandle<SwapchainHandle>(m_swapchains.size());
    }

    auto swapchain = std::make_unique<OpenGLSwapchain>(*this, handle, surface, desc);
    if (!swapchain->initialize()) {
        return {};
    }

    if (handle.value > m_swapchains.size()) {
        m_swapchains.push_back(std::move(swapchain));
    } else {
        m_swapchains[handleIndex(handle)] = std::move(swapchain);
    }

    return handle;
}

void OpenGLDevice::destroySwapchain(SwapchainHandle swapchain)
{
    if (getOpenGLSwapchain(swapchain) == nullptr) {
        return;
    }

    m_swapchains[handleIndex(swapchain)].reset();
}

Swapchain* OpenGLDevice::getSwapchain(SwapchainHandle swapchain)
{
    return getOpenGLSwapchain(swapchain);
}

OpenGLSwapchain* OpenGLDevice::getOpenGLSwapchain(SwapchainHandle handle)
{
    if (!handle || handle.value > m_swapchains.size()) {
        return nullptr;
    }

    return m_swapchains[handleIndex(handle)].get();
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
        if (m_swapchains[i] != nullptr && makeSwapchainCurrent(makeHandle<SwapchainHandle>(i))) {
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
                                 SurfaceHandle surface,
                                 const SwapchainDesc& desc)
    : m_device(device),
      m_handle(handle),
      m_surface(surface),
      m_desc(desc)
{}

OpenGLSwapchain::~OpenGLSwapchain()
{
    if (m_color_view) {
        m_device.destroyTextureView(m_color_view);
        m_color_view = {};
    }

    if (m_depth_stencil_view) {
        m_device.destroyTextureView(m_depth_stencil_view);
        m_depth_stencil_view = {};
    }

    m_device.releaseNativeSwapchain(m_native);
}

bool OpenGLSwapchain::initialize()
{
    auto* surface = m_device.getSurface(m_surface);
    if (surface == nullptr) {
        return false;
    }

    if (!createOpenGLNativeSwapchain(surface->getNativeHandle(), m_native)) {
        return false;
    }

    if (!m_device.ensureContextForSwapchain(*this, m_desc.vsync)) {
        return false;
    }

    m_color_view = m_device.createSwapchainTextureView(m_desc.color_format, m_handle);
    if (m_desc.enable_depth_stencil) {
        m_depth_stencil_view = m_device.createSwapchainTextureView(m_desc.depth_stencil_format, m_handle);
    }

    resize(surface->getWidth(), surface->getHeight());
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
