#pragma once
#include "bind_group.h"
#include "rhi_types.h"
#include "shader.h"
#include "texture.h"

#include <cstdint>
#include <vector>

namespace lunalite::rhi {

enum class PrimitiveTopology {
    Triangle,
    Line,
    Point
};

enum class CullMode {
    None,
    Front,
    Back
};

enum class FrontFace {
    Clockwise,
    CounterClockwise
};

enum class CompareOp {
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always
};

enum class BlendFactor {
    Zero,
    One,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha
};

enum class BlendOp {
    Add,
    Subtract,
    ReverseSubtract
};

enum class VertexFormat {
    Float1,
    Float2,
    Float3,
    Float4,
    Int1,
    Int2,
    Int3,
    Int4,
    UInt1,
    UInt2,
    UInt3,
    UInt4,
    Bool1,
    Bool2,
    Bool3,
    Bool4,
    Byte,
    RGBA8Unorm
};

enum class VertexStepMode {
    Vertex,
    Instance
};

struct VertexAttributeDesc {
    uint32_t location{0};
    VertexFormat format{VertexFormat::Float1};
    uint32_t offset{0};
};

struct VertexBufferLayoutDesc {
    uint32_t binding{0};
    uint32_t stride{0};
    VertexStepMode step_mode{VertexStepMode::Vertex};
    std::vector<VertexAttributeDesc> attributes;
};

struct VertexInputDesc {
    std::vector<VertexBufferLayoutDesc> buffers;
};

struct DepthState {
    bool enabled{true};
    bool write_enabled{true};
    CompareOp compare{CompareOp::Less};
};

struct RasterState {
    CullMode cull_mode{CullMode::None};
    FrontFace front_face{FrontFace::CounterClockwise};
};

struct BlendState {
    bool enabled{false};
    BlendFactor src_color{BlendFactor::SrcAlpha};
    BlendFactor dst_color{BlendFactor::OneMinusSrcAlpha};
    BlendOp color_op{BlendOp::Add};
    BlendFactor src_alpha{BlendFactor::One};
    BlendFactor dst_alpha{BlendFactor::OneMinusSrcAlpha};
    BlendOp alpha_op{BlendOp::Add};
};

enum class ColorWriteMask : uint32_t {
    None = 0,
    R = 1 << 0,
    G = 1 << 1,
    B = 1 << 2,
    A = 1 << 3,
    All = 0xF
};

constexpr ColorWriteMask operator|(ColorWriteMask lhs, ColorWriteMask rhs)
{
    return static_cast<ColorWriteMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr ColorWriteMask operator&(ColorWriteMask lhs, ColorWriteMask rhs)
{
    return static_cast<ColorWriteMask>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

struct ColorTargetState {
    TextureFormat format{TextureFormat::RGBA8_UNorm};
    BlendState blend{};
    ColorWriteMask write_mask{ColorWriteMask::All};
};

struct RenderTargetState {
    std::vector<ColorTargetState> color_targets;
    bool has_depth_stencil{false};
    TextureFormat depth_stencil_format{TextureFormat::Depth24Stencil8};
};

struct PushConstantRange {
    ShaderStageFlags stages{shaderStageFlag(ShaderStage::Vertex) | shaderStageFlag(ShaderStage::Fragment)};
    uint32_t offset{0};
    uint32_t size{0};
};

struct PipelineLayoutDesc {
    std::vector<BindGroupLayoutHandle> bind_group_layouts;
    std::vector<PushConstantRange> push_constants;
};

struct PipelineDesc {
    PrimitiveTopology topology;
    VertexInputDesc vertex_input;
    PipelineLayoutHandle layout{};
    ShaderHandle vertex_shader;
    ShaderHandle fragment_shader;
    RenderTargetState render_target_state{};
    DepthState depth_state{};
    RasterState raster_state{};
};

} // namespace lunalite::rhi
