#pragma once
#include "../interface/swapchain.h"

namespace lunalite::rhi {
class OpenGLDevice;
class Surface;

class OpenGLSwapchain final : public Swapchain {
public:
    OpenGLSwapchain(OpenGLDevice& device, Surface& surface);
    ~OpenGLSwapchain() override = default;

    TextureViewHandle getCurrentColorTextureView() const override;
    TextureViewHandle getDepthStencilTextureView() const override;
    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    void resize(uint32_t width, uint32_t height) override;
    void present() override;

private:
    OpenGLDevice& m_device;
    Surface& m_surface;
    TextureViewHandle m_color_view{0};
    TextureViewHandle m_depth_stencil_view{0};
    uint32_t m_width{0};
    uint32_t m_height{0};
};
} // namespace lunalite::rhi
