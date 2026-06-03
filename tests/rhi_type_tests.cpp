#include "test_framework.h"
#include "TinyRHI/interface/buffer.h"
#include "TinyRHI/interface/pipeline.h"
#include "TinyRHI/interface/shader.h"
#include "TinyRHI/interface/texture.h"

using namespace lunalite::rhi;

TINYRHI_TEST_CASE("shader stage flags can be combined")
{
    const ShaderStageFlags flags = ShaderStage::Vertex | ShaderStage::Fragment | shaderStageFlag(ShaderStage::Compute);

    TINYRHI_CHECK((flags & shaderStageFlag(ShaderStage::Vertex)) != 0);
    TINYRHI_CHECK((flags & shaderStageFlag(ShaderStage::Fragment)) != 0);
    TINYRHI_CHECK((flags & shaderStageFlag(ShaderStage::Compute)) != 0);
}

TINYRHI_TEST_CASE("texture usage flags preserve individual bits")
{
    TextureUsage usage = TextureUsage::RenderTarget;
    usage |= TextureUsage::Sampled;
    usage |= TextureUsage::Storage;

    TINYRHI_CHECK((usage & TextureUsage::RenderTarget) == TextureUsage::RenderTarget);
    TINYRHI_CHECK((usage & TextureUsage::Sampled) == TextureUsage::Sampled);
    TINYRHI_CHECK((usage & TextureUsage::Storage) == TextureUsage::Storage);
    TINYRHI_CHECK((usage & TextureUsage::DepthStencil) == TextureUsage::None);
}

TINYRHI_TEST_CASE("texture aspect depth stencil contains depth and stencil")
{
    TINYRHI_CHECK((TextureAspect::DepthStencil & TextureAspect::Depth) == TextureAspect::Depth);
    TINYRHI_CHECK((TextureAspect::DepthStencil & TextureAspect::Stencil) == TextureAspect::Stencil);
    TINYRHI_CHECK((TextureAspect::DepthStencil & TextureAspect::Color) == TextureAspect::None);
}

TINYRHI_TEST_CASE("texture cube descriptors normalize and validate")
{
    const TextureDesc texture2D{};
    TINYRHI_CHECK(textureDescValid(texture2D));

    TextureDesc texture2DArray = texture2D;
    texture2DArray.array_layers = 6;
    TINYRHI_CHECK(!textureDescValid(texture2DArray));

    TextureDesc cubeDesc = normalizeTextureDesc(TextureDesc{
        .width = 128,
        .height = 128,
        .dimension = TextureDimension::TextureCube,
        .format = TextureFormat::RGBA16F,
        .usage = TextureUsage::Sampled | TextureUsage::RenderTarget,
        .mip_levels = 5,
    });
    TINYRHI_CHECK(cubeDesc.array_layers == 6);
    TINYRHI_CHECK(textureDescValid(cubeDesc));

    TextureDesc nonsquareCube = cubeDesc;
    nonsquareCube.height = 64;
    TINYRHI_CHECK(!textureDescValid(nonsquareCube));

    TextureDesc shortCube = cubeDesc;
    shortCube.array_layers = 5;
    TINYRHI_CHECK(!textureDescValid(shortCube));

    TINYRHI_CHECK(textureViewDescValid(cubeDesc,
                                       TextureViewDesc{
                                           .view_dimension = TextureViewDimension::TextureCube,
                                           .format = TextureFormat::RGBA16F,
                                           .mip_level_count = 5,
                                           .array_layer_count = 6,
                                       }));
    TINYRHI_CHECK(textureViewDescValid(cubeDesc,
                                       TextureViewDesc{
                                           .view_dimension = TextureViewDimension::Texture2D,
                                           .format = TextureFormat::RGBA16F,
                                           .base_mip_level = 2,
                                           .mip_level_count = 1,
                                           .base_array_layer = 3,
                                           .array_layer_count = 1,
                                       }));
    TINYRHI_CHECK(!textureViewDescValid(cubeDesc,
                                        TextureViewDesc{
                                            .view_dimension = TextureViewDimension::TextureCube,
                                            .format = TextureFormat::RGBA16F,
                                            .base_array_layer = 1,
                                            .array_layer_count = 6,
                                        }));
    TINYRHI_CHECK(!textureViewDescValid(cubeDesc,
                                        TextureViewDesc{
                                            .view_dimension = TextureViewDimension::Texture2D,
                                            .format = TextureFormat::RGBA16F,
                                            .base_array_layer = 3,
                                            .array_layer_count = 2,
                                        }));
}

TINYRHI_TEST_CASE("buffer usage flags preserve individual bits")
{
    BufferUsage usage = BufferUsage::Vertex;
    usage |= BufferUsage::CopyDst;

    TINYRHI_CHECK((usage & BufferUsage::Vertex) == BufferUsage::Vertex);
    TINYRHI_CHECK((usage & BufferUsage::CopyDst) == BufferUsage::CopyDst);
    TINYRHI_CHECK((usage & BufferUsage::Uniform) == BufferUsage::None);
}

TINYRHI_TEST_CASE("color write mask can select individual channels")
{
    const ColorWriteMask mask = ColorWriteMask::R | ColorWriteMask::B;

    TINYRHI_CHECK((mask & ColorWriteMask::R) == ColorWriteMask::R);
    TINYRHI_CHECK((mask & ColorWriteMask::B) == ColorWriteMask::B);
    TINYRHI_CHECK((mask & ColorWriteMask::G) == ColorWriteMask::None);
    TINYRHI_CHECK((mask & ColorWriteMask::A) == ColorWriteMask::None);
}

TINYRHI_TEST_CASE("pipeline state defaults are renderable")
{
    const DepthState depth{};
    const RasterState raster{};
    const BlendState blend{};
    const ColorTargetState colorTarget{};

    TINYRHI_CHECK(depth.enabled);
    TINYRHI_CHECK(depth.write_enabled);
    TINYRHI_CHECK(depth.compare == CompareOp::Less);
    TINYRHI_CHECK(raster.cull_mode == CullMode::None);
    TINYRHI_CHECK(raster.front_face == FrontFace::CounterClockwise);
    TINYRHI_CHECK(!blend.enabled);
    TINYRHI_CHECK(colorTarget.format == TextureFormat::RGBA8_UNorm);
    TINYRHI_CHECK(colorTarget.write_mask == ColorWriteMask::All);
}
