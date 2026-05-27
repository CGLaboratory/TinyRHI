#pragma once
#include "../interface/surface.h"
#include "../interface/swapchain.h"
#include "native_swapchain.h"

namespace lunalite::rhi {
class OpenGLDevice;

class OpenGLSwapchain final : public Swapchain {
public:
    OpenGLSwapchain(OpenGLDevice& device, SwapchainHandle handle, SurfaceHandle surface, const SwapchainDesc& desc);
    ~OpenGLSwapchain() override;

    OpenGLSwapchain(const OpenGLSwapchain&) = delete;
    OpenGLSwapchain& operator=(const OpenGLSwapchain&) = delete;

    bool initialize();

    TextureViewHandle getCurrentColorTextureView() const override;
    TextureViewHandle getDepthStencilTextureView() const override;
    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    void resize(uint32_t width, uint32_t height) override;
    void present() override;

    SwapchainHandle handle() const;
    OpenGLNativeSwapchain& nativeSwapchain();

private:
    OpenGLDevice& m_device;
    SwapchainHandle m_handle{};
    SurfaceHandle m_surface{};
    SwapchainDesc m_desc{};
    OpenGLNativeSwapchain m_native{};
    TextureViewHandle m_color_view{};
    TextureViewHandle m_depth_stencil_view{};
    uint32_t m_width{0};
    uint32_t m_height{0};
};
} // namespace lunalite::rhi
