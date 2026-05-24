#include "gl_convert.h"

namespace lunalite::rhi {

GLenum toGLBufferUsage(BufferUsage usage)
{
    switch (usage) {
        case BufferUsage::Static:
            return GL_STATIC_DRAW;
        case BufferUsage::Dynamic:
            return GL_DYNAMIC_DRAW;
    }

    return GL_STATIC_DRAW;
}

GLenum toGLIndexFormat(IndexFormat format)
{
    switch (format) {
        case IndexFormat::UInt16:
            return GL_UNSIGNED_SHORT;
        case IndexFormat::UInt32:
            return GL_UNSIGNED_INT;
    }

    return GL_UNSIGNED_INT;
}

GLenum toGLShaderStage(ShaderStage stage)
{
    switch (stage) {
        case ShaderStage::Vertex:
            return GL_VERTEX_SHADER;
        case ShaderStage::Fragment:
            return GL_FRAGMENT_SHADER;
    }

    return GL_VERTEX_SHADER;
}

GLenum toGLTopology(PrimitiveTopology topology)
{
    switch (topology) {
        case PrimitiveTopology::Triangle:
            return GL_TRIANGLES;
        case PrimitiveTopology::Line:
            return GL_LINES;
        case PrimitiveTopology::Point:
            return GL_POINTS;
    }

    return GL_TRIANGLES;
}

GLenum toGLCullMode(CullMode mode)
{
    switch (mode) {
        case CullMode::Front:
            return GL_FRONT;
        case CullMode::Back:
            return GL_BACK;
        case CullMode::None:
            return GL_BACK;
    }

    return GL_BACK;
}

GLenum toGLFrontFace(FrontFace face)
{
    switch (face) {
        case FrontFace::Clockwise:
            return GL_CW;
        case FrontFace::CounterClockwise:
            return GL_CCW;
    }

    return GL_CCW;
}

GLenum toGLCompareOp(CompareOp op)
{
    switch (op) {
        case CompareOp::Never:
            return GL_NEVER;
        case CompareOp::Less:
            return GL_LESS;
        case CompareOp::Equal:
            return GL_EQUAL;
        case CompareOp::LessOrEqual:
            return GL_LEQUAL;
        case CompareOp::Greater:
            return GL_GREATER;
        case CompareOp::NotEqual:
            return GL_NOTEQUAL;
        case CompareOp::GreaterOrEqual:
            return GL_GEQUAL;
        case CompareOp::Always:
            return GL_ALWAYS;
    }

    return GL_LESS;
}

GLenum toGLBlendFactor(BlendFactor factor)
{
    switch (factor) {
        case BlendFactor::Zero:
            return GL_ZERO;
        case BlendFactor::One:
            return GL_ONE;
        case BlendFactor::SrcAlpha:
            return GL_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:
            return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:
            return GL_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:
            return GL_ONE_MINUS_DST_ALPHA;
    }

    return GL_ONE;
}

GLenum toGLBlendOp(BlendOp op)
{
    switch (op) {
        case BlendOp::Add:
            return GL_FUNC_ADD;
        case BlendOp::Subtract:
            return GL_FUNC_SUBTRACT;
        case BlendOp::ReverseSubtract:
            return GL_FUNC_REVERSE_SUBTRACT;
    }

    return GL_FUNC_ADD;
}

GLenum toGLFilterMode(FilterMode mode)
{
    switch (mode) {
        case FilterMode::Nearest:
            return GL_NEAREST;
        case FilterMode::Linear:
            return GL_LINEAR;
    }

    return GL_LINEAR;
}

GLenum toGLAddressMode(AddressMode mode)
{
    switch (mode) {
        case AddressMode::Repeat:
            return GL_REPEAT;
        case AddressMode::ClampToEdge:
            return GL_CLAMP_TO_EDGE;
    }

    return GL_CLAMP_TO_EDGE;
}

GLenum toGLTextureInternalFormat(TextureFormat format)
{
    switch (format) {
        case TextureFormat::RGBA8:
            return GL_RGBA8;
        case TextureFormat::RGBA16F:
            return GL_RGBA16F;
        case TextureFormat::RGBA32F:
            return GL_RGBA32F;
        case TextureFormat::Depth24Stencil8:
            return GL_DEPTH24_STENCIL8;
        case TextureFormat::Depth32F:
            return GL_DEPTH_COMPONENT32F;
    }

    return GL_RGBA8;
}

GLenum toGLTextureUploadFormat(TextureFormat format)
{
    switch (format) {
        case TextureFormat::RGBA8:
        case TextureFormat::RGBA16F:
        case TextureFormat::RGBA32F:
            return GL_RGBA;
        case TextureFormat::Depth24Stencil8:
            return GL_DEPTH_STENCIL;
        case TextureFormat::Depth32F:
            return GL_DEPTH_COMPONENT;
    }

    return GL_RGBA;
}

GLenum toGLTextureUploadType(TextureFormat format)
{
    switch (format) {
        case TextureFormat::RGBA8:
            return GL_UNSIGNED_BYTE;
        case TextureFormat::RGBA16F:
            return GL_HALF_FLOAT;
        case TextureFormat::RGBA32F:
            return GL_FLOAT;
        case TextureFormat::Depth24Stencil8:
            return GL_UNSIGNED_INT_24_8;
        case TextureFormat::Depth32F:
            return GL_FLOAT;
    }

    return GL_UNSIGNED_BYTE;
}

GLenum toGLAttachment(TextureFormat format)
{
    switch (format) {
        case TextureFormat::Depth24Stencil8:
            return GL_DEPTH_STENCIL_ATTACHMENT;
        case TextureFormat::Depth32F:
            return GL_DEPTH_ATTACHMENT;
        case TextureFormat::RGBA8:
        case TextureFormat::RGBA16F:
        case TextureFormat::RGBA32F:
            return GL_COLOR_ATTACHMENT0;
    }

    return GL_COLOR_ATTACHMENT0;
}

bool isDepthFormat(TextureFormat format)
{
    return format == TextureFormat::Depth24Stencil8 || format == TextureFormat::Depth32F;
}

uint32_t vertexFormatComponentCount(VertexFormat format)
{
    switch (format) {
        case VertexFormat::Float1:
        case VertexFormat::Int1:
        case VertexFormat::UInt1:
        case VertexFormat::Bool1:
        case VertexFormat::Byte:
            return 1;
        case VertexFormat::Float2:
        case VertexFormat::Int2:
        case VertexFormat::UInt2:
        case VertexFormat::Bool2:
            return 2;
        case VertexFormat::Float3:
        case VertexFormat::Int3:
        case VertexFormat::UInt3:
        case VertexFormat::Bool3:
            return 3;
        case VertexFormat::Float4:
        case VertexFormat::Int4:
        case VertexFormat::UInt4:
        case VertexFormat::Bool4:
            return 4;
    }

    return 1;
}

GLenum vertexFormatType(VertexFormat format)
{
    switch (format) {
        case VertexFormat::Float1:
        case VertexFormat::Float2:
        case VertexFormat::Float3:
        case VertexFormat::Float4:
            return GL_FLOAT;
        case VertexFormat::Int1:
        case VertexFormat::Int2:
        case VertexFormat::Int3:
        case VertexFormat::Int4:
            return GL_INT;
        case VertexFormat::UInt1:
        case VertexFormat::UInt2:
        case VertexFormat::UInt3:
        case VertexFormat::UInt4:
            return GL_UNSIGNED_INT;
        case VertexFormat::Bool1:
        case VertexFormat::Bool2:
        case VertexFormat::Bool3:
        case VertexFormat::Bool4:
            return GL_BOOL;
        case VertexFormat::Byte:
            return GL_BYTE;
    }

    return GL_FLOAT;
}

bool isIntegerVertexFormat(VertexFormat format)
{
    switch (format) {
        case VertexFormat::Int1:
        case VertexFormat::Int2:
        case VertexFormat::Int3:
        case VertexFormat::Int4:
        case VertexFormat::UInt1:
        case VertexFormat::UInt2:
        case VertexFormat::UInt3:
        case VertexFormat::UInt4:
        case VertexFormat::Bool1:
        case VertexFormat::Bool2:
        case VertexFormat::Bool3:
        case VertexFormat::Bool4:
        case VertexFormat::Byte:
            return true;
        case VertexFormat::Float1:
        case VertexFormat::Float2:
        case VertexFormat::Float3:
        case VertexFormat::Float4:
            return false;
    }

    return false;
}

GLuint vertexAttributeLocation(VertexAttribute semantic)
{
    switch (semantic) {
        case VertexAttribute::Position:
            return 0;
        case VertexAttribute::Normal:
            return 1;
        case VertexAttribute::TexCoord:
            return 2;
        case VertexAttribute::Color:
            return 3;
    }

    return 0;
}

} // namespace lunalite::rhi
