#pragma once
#include "rhi_types.h"

#include <cstddef>
#include <cstdint>

namespace lunalite::rhi {

enum class TextureFormat {
    RGBA8,
    RGBA16F,
    RGBA32F,
    Depth24Stencil8,
    Depth32F
};

enum class TextureUsage : uint32_t {
    None = 0,
    RenderTarget = 1 << 0,
    DepthStencil = 1 << 1,
    Sampled = 1 << 2,
    CopySrc = 1 << 3,
    CopyDst = 1 << 4
};

constexpr TextureUsage operator|(TextureUsage lhs, TextureUsage rhs)
{
    return static_cast<TextureUsage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr TextureUsage operator&(TextureUsage lhs, TextureUsage rhs)
{
    return static_cast<TextureUsage>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr TextureUsage& operator|=(TextureUsage& lhs, TextureUsage rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

enum class TextureAspect : uint32_t {
    None = 0,
    Color = 1 << 0,
    Depth = 1 << 1,
    Stencil = 1 << 2,
    DepthStencil = Depth | Stencil
};

constexpr TextureAspect operator|(TextureAspect lhs, TextureAspect rhs)
{
    return static_cast<TextureAspect>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr TextureAspect operator&(TextureAspect lhs, TextureAspect rhs)
{
    return static_cast<TextureAspect>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

struct TextureDesc {
    uint32_t width{1};
    uint32_t height{1};
    TextureFormat format{TextureFormat::RGBA8};
    TextureUsage usage{TextureUsage::Sampled};
    uint32_t mip_levels{1};
};

struct TextureViewDesc {
    TextureHandle texture{0};
    TextureFormat format{TextureFormat::RGBA8};
    TextureAspect aspect{TextureAspect::Color};
    uint32_t base_mip_level{0};
    uint32_t mip_level_count{1};
    uint32_t base_array_layer{0};
    uint32_t array_layer_count{1};
};

struct TextureUploadDesc {
    uint32_t x{0};
    uint32_t y{0};
    uint32_t width{0};
    uint32_t height{0};
    TextureFormat format{TextureFormat::RGBA8};
    const void* data{nullptr};
    size_t row_pitch{0};
};

} // namespace lunalite::rhi
