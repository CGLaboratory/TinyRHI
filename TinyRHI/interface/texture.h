#pragma once
#include "rhi_types.h"

#include <cstddef>
#include <cstdint>

namespace lunalite::rhi {

enum class TextureFormat {
    RGBA8_UNorm,
    RGBA8_SRGB,
    RGBA16F,
    RGBA32F,
    Depth24Stencil8,
    Depth32F
};

enum class TextureDimension {
    Texture2D,
    TextureCube
};

enum class TextureViewDimension {
    Texture2D,
    TextureCube
};

enum class TextureUsage : uint32_t {
    None = 0,
    RenderTarget = 1 << 0,
    DepthStencil = 1 << 1,
    Sampled = 1 << 2,
    CopySrc = 1 << 3,
    CopyDst = 1 << 4,
    Storage = 1 << 5
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
    TextureDimension dimension{TextureDimension::Texture2D};
    TextureFormat format{TextureFormat::RGBA8_UNorm};
    TextureUsage usage{TextureUsage::Sampled};
    uint32_t mip_levels{1};
    uint32_t array_layers{1};
};

struct TextureViewDesc {
    TextureHandle texture{};
    TextureViewDimension view_dimension{TextureViewDimension::Texture2D};
    TextureFormat format{TextureFormat::RGBA8_UNorm};
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
    uint32_t mip_level{0};
    uint32_t array_layer{0};
    TextureFormat format{TextureFormat::RGBA8_UNorm};
    const void* data{nullptr};
    size_t row_pitch{0};
};

inline TextureDesc normalizeTextureDesc(TextureDesc desc)
{
    if (desc.dimension == TextureDimension::TextureCube && desc.array_layers == 1) {
        desc.array_layers = 6;
    }

    return desc;
}

inline bool textureDescValid(const TextureDesc& desc)
{
    if (desc.width == 0 || desc.height == 0 || desc.mip_levels == 0 || desc.array_layers == 0) {
        return false;
    }

    switch (desc.dimension) {
        case TextureDimension::Texture2D:
            return desc.array_layers == 1;
        case TextureDimension::TextureCube:
            return desc.width == desc.height && desc.array_layers == 6;
    }

    return false;
}

inline bool textureViewDescValid(const TextureDesc& textureDesc, const TextureViewDesc& viewDesc)
{
    if (viewDesc.mip_level_count == 0 || viewDesc.array_layer_count == 0 ||
        viewDesc.base_mip_level >= textureDesc.mip_levels ||
        viewDesc.mip_level_count > textureDesc.mip_levels - viewDesc.base_mip_level ||
        viewDesc.base_array_layer >= textureDesc.array_layers ||
        viewDesc.array_layer_count > textureDesc.array_layers - viewDesc.base_array_layer) {
        return false;
    }

    switch (textureDesc.dimension) {
        case TextureDimension::Texture2D:
            return viewDesc.view_dimension == TextureViewDimension::Texture2D && viewDesc.base_array_layer == 0 &&
                   viewDesc.array_layer_count == 1;
        case TextureDimension::TextureCube:
            if (viewDesc.view_dimension == TextureViewDimension::TextureCube) {
                return viewDesc.base_array_layer == 0 && viewDesc.array_layer_count == 6;
            }

            return viewDesc.view_dimension == TextureViewDimension::Texture2D && viewDesc.array_layer_count == 1;
    }

    return false;
}

} // namespace lunalite::rhi
