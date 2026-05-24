#include "test_framework.h"

#include "TinyRHI/interface/pipeline.h"
#include "TinyRHI/interface/shader.h"
#include "TinyRHI/interface/texture.h"

using namespace lunalite::rhi;

TINYRHI_TEST_CASE("shader stage flags can be combined")
{
    const ShaderStageFlags flags = ShaderStage::Vertex | ShaderStage::Fragment;

    TINYRHI_CHECK((flags & shaderStageFlag(ShaderStage::Vertex)) != 0);
    TINYRHI_CHECK((flags & shaderStageFlag(ShaderStage::Fragment)) != 0);
}

TINYRHI_TEST_CASE("texture usage flags preserve individual bits")
{
    TextureUsage usage = TextureUsage::RenderTarget;
    usage |= TextureUsage::Sampled;

    TINYRHI_CHECK((usage & TextureUsage::RenderTarget) == TextureUsage::RenderTarget);
    TINYRHI_CHECK((usage & TextureUsage::Sampled) == TextureUsage::Sampled);
    TINYRHI_CHECK((usage & TextureUsage::DepthStencil) == TextureUsage::None);
}

TINYRHI_TEST_CASE("texture aspect depth stencil contains depth and stencil")
{
    TINYRHI_CHECK((TextureAspect::DepthStencil & TextureAspect::Depth) == TextureAspect::Depth);
    TINYRHI_CHECK((TextureAspect::DepthStencil & TextureAspect::Stencil) == TextureAspect::Stencil);
    TINYRHI_CHECK((TextureAspect::DepthStencil & TextureAspect::Color) == TextureAspect::None);
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
    TINYRHI_CHECK(colorTarget.format == TextureFormat::RGBA8);
    TINYRHI_CHECK(colorTarget.write_mask == ColorWriteMask::All);
}
