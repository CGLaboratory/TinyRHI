#pragma once
#include "../interface/buffer.h"
#include "../interface/pipeline.h"
#include "../interface/sampler.h"
#include "../interface/shader.h"
#include "../interface/texture.h"

#include <cstdint>

#include <glad/glad.h>

namespace lunalite::rhi {

GLenum toGLBufferUsage(MemoryUsage usage);
GLenum toGLIndexFormat(IndexFormat format);
GLenum toGLShaderStage(ShaderStage stage);
GLenum toGLTopology(PrimitiveTopology topology);
GLenum toGLCullMode(CullMode mode);
GLenum toGLFrontFace(FrontFace face);
GLenum toGLCompareOp(CompareOp op);
GLenum toGLBlendFactor(BlendFactor factor);
GLenum toGLBlendOp(BlendOp op);
GLenum toGLFilterMode(FilterMode mode);
GLenum toGLMinFilter(FilterMode minFilter, MipFilter mipFilter);
GLenum toGLAddressMode(AddressMode mode);
GLenum toGLTextureTarget(TextureDimension dimension);
GLenum toGLTextureInternalFormat(TextureFormat format);
GLenum toGLTextureUploadFormat(TextureFormat format);
GLenum toGLTextureUploadType(TextureFormat format);
GLenum toGLAttachment(TextureFormat format);
bool isDepthFormat(TextureFormat format);
bool isSRGBFormat(TextureFormat format);
uint32_t vertexFormatComponentCount(VertexFormat format);
GLenum vertexFormatType(VertexFormat format);
bool isIntegerVertexFormat(VertexFormat format);
bool isNormalizedVertexFormat(VertexFormat format);

} // namespace lunalite::rhi
