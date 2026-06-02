#include "test_framework.h"

#include "TinyRHI/opengl/gl_convert.h"

using namespace lunalite::rhi;

TINYRHI_TEST_CASE("buffer and index formats map to OpenGL enums")
{
    TINYRHI_CHECK(toGLBufferUsage(MemoryUsage::GpuOnly) == GL_STATIC_DRAW);
    TINYRHI_CHECK(toGLBufferUsage(MemoryUsage::CpuToGpu) == GL_DYNAMIC_DRAW);
    TINYRHI_CHECK(toGLBufferUsage(MemoryUsage::GpuToCpu) == GL_DYNAMIC_DRAW);
    TINYRHI_CHECK(toGLIndexFormat(IndexFormat::UInt16) == GL_UNSIGNED_SHORT);
    TINYRHI_CHECK(toGLIndexFormat(IndexFormat::UInt32) == GL_UNSIGNED_INT);
}

TINYRHI_TEST_CASE("shader stages and primitive topologies map to OpenGL enums")
{
    TINYRHI_CHECK(toGLShaderStage(ShaderStage::Vertex) == GL_VERTEX_SHADER);
    TINYRHI_CHECK(toGLShaderStage(ShaderStage::Fragment) == GL_FRAGMENT_SHADER);
    TINYRHI_CHECK(toGLTopology(PrimitiveTopology::Triangle) == GL_TRIANGLES);
    TINYRHI_CHECK(toGLTopology(PrimitiveTopology::Line) == GL_LINES);
    TINYRHI_CHECK(toGLTopology(PrimitiveTopology::Point) == GL_POINTS);
}

TINYRHI_TEST_CASE("render states map to OpenGL enums")
{
    TINYRHI_CHECK(toGLCullMode(CullMode::Front) == GL_FRONT);
    TINYRHI_CHECK(toGLCullMode(CullMode::Back) == GL_BACK);
    TINYRHI_CHECK(toGLFrontFace(FrontFace::Clockwise) == GL_CW);
    TINYRHI_CHECK(toGLFrontFace(FrontFace::CounterClockwise) == GL_CCW);
    TINYRHI_CHECK(toGLCompareOp(CompareOp::LessOrEqual) == GL_LEQUAL);
    TINYRHI_CHECK(toGLCompareOp(CompareOp::Always) == GL_ALWAYS);
}

TINYRHI_TEST_CASE("texture formats map to OpenGL storage and upload enums")
{
    TINYRHI_CHECK(toGLTextureTarget(TextureDimension::Texture2D) == GL_TEXTURE_2D);
    TINYRHI_CHECK(toGLTextureTarget(TextureDimension::TextureCube) == GL_TEXTURE_CUBE_MAP);

    TINYRHI_CHECK(toGLTextureInternalFormat(TextureFormat::RGBA8_UNorm) == GL_RGBA8);
    TINYRHI_CHECK(toGLTextureInternalFormat(TextureFormat::RGBA8_SRGB) == GL_SRGB8_ALPHA8);
    TINYRHI_CHECK(toGLTextureInternalFormat(TextureFormat::RGBA16F) == GL_RGBA16F);
    TINYRHI_CHECK(toGLTextureInternalFormat(TextureFormat::RGBA32F) == GL_RGBA32F);
    TINYRHI_CHECK(toGLTextureInternalFormat(TextureFormat::Depth24Stencil8) == GL_DEPTH24_STENCIL8);
    TINYRHI_CHECK(toGLTextureInternalFormat(TextureFormat::Depth32F) == GL_DEPTH_COMPONENT32F);

    TINYRHI_CHECK(toGLTextureUploadFormat(TextureFormat::RGBA8_UNorm) == GL_RGBA);
    TINYRHI_CHECK(toGLTextureUploadFormat(TextureFormat::RGBA8_SRGB) == GL_RGBA);
    TINYRHI_CHECK(toGLTextureUploadFormat(TextureFormat::RGBA16F) == GL_RGBA);
    TINYRHI_CHECK(toGLTextureUploadFormat(TextureFormat::RGBA32F) == GL_RGBA);
    TINYRHI_CHECK(toGLTextureUploadFormat(TextureFormat::Depth24Stencil8) == GL_DEPTH_STENCIL);
    TINYRHI_CHECK(toGLTextureUploadFormat(TextureFormat::Depth32F) == GL_DEPTH_COMPONENT);
    TINYRHI_CHECK(toGLTextureUploadType(TextureFormat::RGBA8_UNorm) == GL_UNSIGNED_BYTE);
    TINYRHI_CHECK(toGLTextureUploadType(TextureFormat::RGBA8_SRGB) == GL_UNSIGNED_BYTE);
    TINYRHI_CHECK(toGLTextureUploadType(TextureFormat::RGBA16F) == GL_HALF_FLOAT);
    TINYRHI_CHECK(toGLTextureUploadType(TextureFormat::RGBA32F) == GL_FLOAT);
    TINYRHI_CHECK(toGLTextureUploadType(TextureFormat::Depth24Stencil8) == GL_UNSIGNED_INT_24_8);
    TINYRHI_CHECK(toGLTextureUploadType(TextureFormat::Depth32F) == GL_FLOAT);
    TINYRHI_CHECK(!isSRGBFormat(TextureFormat::RGBA8_UNorm));
    TINYRHI_CHECK(isSRGBFormat(TextureFormat::RGBA8_SRGB));
    TINYRHI_CHECK(!isSRGBFormat(TextureFormat::Depth24Stencil8));
}

TINYRHI_TEST_CASE("sampler modes map to OpenGL enums")
{
    TINYRHI_CHECK(toGLAddressMode(AddressMode::Repeat) == GL_REPEAT);
    TINYRHI_CHECK(toGLAddressMode(AddressMode::ClampToEdge) == GL_CLAMP_TO_EDGE);
    TINYRHI_CHECK(toGLAddressMode(AddressMode::MirroredRepeat) == GL_MIRRORED_REPEAT);
    TINYRHI_CHECK(toGLMinFilter(FilterMode::Nearest, MipFilter::None) == GL_NEAREST);
    TINYRHI_CHECK(toGLMinFilter(FilterMode::Linear, MipFilter::None) == GL_LINEAR);
    TINYRHI_CHECK(toGLMinFilter(FilterMode::Nearest, MipFilter::Nearest) == GL_NEAREST_MIPMAP_NEAREST);
    TINYRHI_CHECK(toGLMinFilter(FilterMode::Linear, MipFilter::Nearest) == GL_LINEAR_MIPMAP_NEAREST);
    TINYRHI_CHECK(toGLMinFilter(FilterMode::Nearest, MipFilter::Linear) == GL_NEAREST_MIPMAP_LINEAR);
    TINYRHI_CHECK(toGLMinFilter(FilterMode::Linear, MipFilter::Linear) == GL_LINEAR_MIPMAP_LINEAR);
}

TINYRHI_TEST_CASE("vertex formats describe locations and component layouts")
{
    TINYRHI_CHECK(vertexFormatComponentCount(VertexFormat::Float1) == 1);
    TINYRHI_CHECK(vertexFormatComponentCount(VertexFormat::Float2) == 2);
    TINYRHI_CHECK(vertexFormatComponentCount(VertexFormat::Float3) == 3);
    TINYRHI_CHECK(vertexFormatComponentCount(VertexFormat::Float4) == 4);
    TINYRHI_CHECK(vertexFormatComponentCount(VertexFormat::RGBA8Unorm) == 4);
    TINYRHI_CHECK(vertexFormatType(VertexFormat::Float3) == GL_FLOAT);
    TINYRHI_CHECK(vertexFormatType(VertexFormat::UInt4) == GL_UNSIGNED_INT);
    TINYRHI_CHECK(vertexFormatType(VertexFormat::RGBA8Unorm) == GL_UNSIGNED_BYTE);
    TINYRHI_CHECK(!isIntegerVertexFormat(VertexFormat::Float4));
    TINYRHI_CHECK(!isIntegerVertexFormat(VertexFormat::RGBA8Unorm));
    TINYRHI_CHECK(isNormalizedVertexFormat(VertexFormat::RGBA8Unorm));
    TINYRHI_CHECK(!isNormalizedVertexFormat(VertexFormat::Float4));
    TINYRHI_CHECK(isIntegerVertexFormat(VertexFormat::Int4));
    TINYRHI_CHECK(isIntegerVertexFormat(VertexFormat::UInt4));
    TINYRHI_CHECK(isIntegerVertexFormat(VertexFormat::Byte));
}
