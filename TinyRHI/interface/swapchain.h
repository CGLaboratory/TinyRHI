#pragma once
#include "rhi_types.h"
#include "texture.h"

#include <cstdint>

namespace lunalite::rhi {
struct SwapchainDesc {
    TextureFormat color_format{TextureFormat::RGBA8_UNorm};
    TextureFormat depth_stencil_format{TextureFormat::Depth24Stencil8};
    bool enable_depth_stencil{true};
    bool vsync{true};
};

class Swapchain {
public:
    virtual ~Swapchain() = default;

    virtual TextureViewHandle getCurrentColorTextureView() const = 0;
    virtual TextureViewHandle getDepthStencilTextureView() const = 0;
    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
    virtual void present() = 0;
};
} // namespace lunalite::rhi
