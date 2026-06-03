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

struct SwapchainFrame {
    SwapchainHandle swapchain{};
    TextureViewHandle color_view{};
    TextureViewHandle depth_stencil_view{};
    uint32_t width{0};
    uint32_t height{0};
};

class Swapchain {
public:
    virtual ~Swapchain() = default;

    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
};
} // namespace lunalite::rhi
