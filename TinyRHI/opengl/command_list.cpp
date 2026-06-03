#include "command_list.h"
#include "device.h"
#include "gl_convert.h"

#include <cstdint>
#include <cstdio>

#include <glad/glad.h>
#include <vector>

namespace lunalite::rhi {

OpenGLCommandList::OpenGLCommandList(OpenGLDevice& device)
    : m_device(device)
{}

namespace {
bool hasUsage(BufferUsage usage, BufferUsage required)
{
    return (usage & required) == required;
}

bool hasUsage(TextureUsage usage, TextureUsage required)
{
    return (usage & required) == required;
}

size_t textureFormatBytesPerPixel(TextureFormat format)
{
    switch (format) {
        case TextureFormat::RGBA8_UNorm:
        case TextureFormat::RGBA8_SRGB:
            return 4;
        case TextureFormat::RG16F:
            return 4;
        case TextureFormat::RG32F:
            return 8;
        case TextureFormat::RGBA16F:
            return 8;
        case TextureFormat::RGBA32F:
            return 16;
        case TextureFormat::Depth24Stencil8:
            return 4;
        case TextureFormat::Depth32F:
            return 4;
    }

    return 4;
}

uint32_t textureMipDimension(uint32_t baseDimension, uint32_t mipLevel)
{
    uint32_t dimension = baseDimension;
    for (uint32_t level = 0; level < mipLevel && dimension > 1; ++level) {
        dimension /= 2;
    }

    return dimension == 0 ? 1 : dimension;
}

const BindGroupLayoutEntry* findLayoutEntry(const BindGroupLayoutDesc& layout, uint32_t binding)
{
    for (const auto& entry : layout.entries) {
        if (entry.binding == binding) {
            return &entry;
        }
    }

    return nullptr;
}

uint32_t dynamicOffsetCount(const BindGroupLayoutDesc& layout)
{
    uint32_t count = 0;
    for (const auto& entry : layout.entries) {
        if (entry.dynamic_offset) {
            ++count;
        }
    }

    return count;
}

size_t dynamicOffsetForBinding(const BindGroupLayoutDesc& layout, uint32_t binding, const uint32_t* dynamic_offsets)
{
    uint32_t dynamicIndex = 0;
    for (const auto& entry : layout.entries) {
        if (!entry.dynamic_offset) {
            continue;
        }

        if (entry.binding == binding) {
            return dynamic_offsets[dynamicIndex];
        }

        ++dynamicIndex;
    }

    return 0;
}

size_t indexFormatSize(IndexFormat format)
{
    switch (format) {
        case IndexFormat::UInt16:
            return sizeof(uint16_t);
        case IndexFormat::UInt32:
            return sizeof(uint32_t);
    }

    return sizeof(uint32_t);
}

uint32_t flattenedBinding(uint32_t set, uint32_t binding)
{
    return set * 16 + binding;
}

GLboolean colorWriteEnabled(ColorWriteMask mask, ColorWriteMask channel)
{
    return (mask & channel) != ColorWriteMask::None ? GL_TRUE : GL_FALSE;
}

bool pushConstantWriteCovered(const PipelineLayoutDesc& layout, ShaderStageFlags stages, uint32_t offset, uint32_t size)
{
    if (stages == 0 || size == 0) {
        return false;
    }

    const uint32_t writeEnd = offset + size;
    if (writeEnd < offset) {
        return false;
    }

    for (const auto& range : layout.push_constants) {
        const uint32_t rangeEnd = range.offset + range.size;
        if ((stages & ~range.stages) == 0 && offset >= range.offset && writeEnd <= rangeEnd) {
            return true;
        }
    }

    return false;
}

void uploadPushConstantUniforms(GLuint program, uint32_t offset, uint32_t size, const void* data)
{
    if (offset % 16 != 0 || size % 16 != 0) {
        return;
    }

    const GLint location = glGetUniformLocation(program, "uPushConstants");
    if (location < 0) {
        return;
    }

    const auto firstVec4 = static_cast<GLint>(offset / 16);
    const auto vec4Count = static_cast<GLsizei>(size / 16);
    const auto* values = static_cast<const GLfloat*>(data);
    glProgramUniform4fv(program, location + firstVec4, vec4Count, values);
}

bool bufferStateCompatible(const OpenGLBuffer& buffer, ResourceState state)
{
    switch (state) {
        case ResourceState::Undefined:
            return false;
        case ResourceState::CopySrc:
            return hasUsage(buffer.usage, BufferUsage::CopySrc);
        case ResourceState::CopyDst:
            return hasUsage(buffer.usage, BufferUsage::CopyDst);
        case ResourceState::VertexBuffer:
            return hasUsage(buffer.usage, BufferUsage::Vertex);
        case ResourceState::IndexBuffer:
            return hasUsage(buffer.usage, BufferUsage::Index);
        case ResourceState::IndirectArgument:
            return hasUsage(buffer.usage, BufferUsage::Indirect);
        case ResourceState::UniformRead:
            return hasUsage(buffer.usage, BufferUsage::Uniform);
        case ResourceState::ShaderRead:
        case ResourceState::StorageRead:
        case ResourceState::StorageReadWrite:
            return hasUsage(buffer.usage, BufferUsage::Storage);
        case ResourceState::ColorAttachment:
        case ResourceState::DepthStencilRead:
        case ResourceState::DepthStencilWrite:
        case ResourceState::Present:
            return false;
    }

    return false;
}

bool textureStateCompatible(const OpenGLTexture& texture, ResourceState state)
{
    switch (state) {
        case ResourceState::Undefined:
            return false;
        case ResourceState::CopySrc:
            return hasUsage(texture.desc.usage, TextureUsage::CopySrc);
        case ResourceState::CopyDst:
            return hasUsage(texture.desc.usage, TextureUsage::CopyDst);
        case ResourceState::ShaderRead:
            return hasUsage(texture.desc.usage, TextureUsage::Sampled);
        case ResourceState::StorageRead:
        case ResourceState::StorageReadWrite:
            return hasUsage(texture.desc.usage, TextureUsage::Storage) && !isDepthFormat(texture.desc.format) &&
                   !isSRGBFormat(texture.desc.format);
        case ResourceState::ColorAttachment:
            return hasUsage(texture.desc.usage, TextureUsage::RenderTarget) && !isDepthFormat(texture.desc.format);
        case ResourceState::DepthStencilRead:
        case ResourceState::DepthStencilWrite:
            return hasUsage(texture.desc.usage, TextureUsage::DepthStencil) && isDepthFormat(texture.desc.format);
        case ResourceState::Present:
            return texture.is_swapchain_backbuffer;
        case ResourceState::VertexBuffer:
        case ResourceState::IndexBuffer:
        case ResourceState::IndirectArgument:
        case ResourceState::UniformRead:
            return false;
    }

    return false;
}

bool bufferStateUsable(const OpenGLBuffer& buffer, ResourceState required)
{
    return buffer.state == required;
}

bool bufferStorageStateUsable(const OpenGLBuffer& buffer)
{
    return buffer.state == ResourceState::StorageRead || buffer.state == ResourceState::StorageReadWrite;
}

bool textureStateUsable(const OpenGLTexture& texture, ResourceState required)
{
    return texture.state == required;
}

bool bufferWriteState(ResourceState state)
{
    return state == ResourceState::StorageReadWrite || state == ResourceState::CopyDst;
}

bool textureWriteState(ResourceState state)
{
    return state == ResourceState::StorageReadWrite || state == ResourceState::CopyDst ||
           state == ResourceState::ColorAttachment || state == ResourceState::DepthStencilWrite;
}

GLbitfield bufferBarrierBits(ResourceState oldState, ResourceState newState)
{
    if (!bufferWriteState(oldState) && !(oldState == newState && bufferWriteState(newState))) {
        return 0;
    }

    GLbitfield bits = 0;
    if (oldState == ResourceState::StorageReadWrite || newState == ResourceState::StorageRead ||
        newState == ResourceState::StorageReadWrite) {
        bits |= GL_SHADER_STORAGE_BARRIER_BIT;
    }
    if (oldState == ResourceState::CopyDst || newState == ResourceState::CopySrc ||
        newState == ResourceState::CopyDst) {
        bits |= GL_BUFFER_UPDATE_BARRIER_BIT;
    }

    switch (newState) {
        case ResourceState::VertexBuffer:
            bits |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
            break;
        case ResourceState::IndexBuffer:
            bits |= GL_ELEMENT_ARRAY_BARRIER_BIT;
            break;
        case ResourceState::UniformRead:
            bits |= GL_UNIFORM_BARRIER_BIT;
            break;
        case ResourceState::IndirectArgument:
            bits |= GL_COMMAND_BARRIER_BIT;
            break;
        default:
            break;
    }

    return bits;
}

GLbitfield textureBarrierBits(ResourceState oldState, ResourceState newState)
{
    if (!textureWriteState(oldState) && !(oldState == newState && textureWriteState(newState))) {
        return 0;
    }

    GLbitfield bits = 0;
    if (oldState == ResourceState::StorageReadWrite || newState == ResourceState::StorageRead ||
        newState == ResourceState::StorageReadWrite) {
        bits |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
    }
    if (oldState == ResourceState::CopyDst || newState == ResourceState::CopySrc ||
        newState == ResourceState::CopyDst) {
        bits |= GL_TEXTURE_UPDATE_BARRIER_BIT;
    }
    if (oldState == ResourceState::ColorAttachment || oldState == ResourceState::DepthStencilWrite ||
        newState == ResourceState::ColorAttachment || newState == ResourceState::DepthStencilWrite ||
        newState == ResourceState::DepthStencilRead) {
        bits |= GL_FRAMEBUFFER_BARRIER_BIT;
    }
    if (newState == ResourceState::ShaderRead) {
        bits |= GL_TEXTURE_FETCH_BARRIER_BIT;
    }

    return bits;
}

void transitionTexture(OpenGLTexture& texture, ResourceState state)
{
    const GLbitfield bits = textureBarrierBits(texture.state, state);
    if (bits != 0) {
        glMemoryBarrier(bits);
    }
    texture.state = state;
}
} // namespace

void OpenGLCommandList::begin()
{
    m_current_pipeline = {};
    m_current_index_buffer = {};
    m_current_index_format = IndexFormat::UInt32;
    m_current_index_buffer_offset = 0;
}

void OpenGLCommandList::end() {}

void OpenGLCommandList::beginRenderPass(const RenderPassBeginInfo& info)
{
    if (info.color_attachments.empty() && !info.has_depth_stencil_attachment) {
        std::printf("OpenGL render pass begin failed: no attachments.\n");
        return;
    }

    bool uses_swapchain = false;
    bool uses_offscreen_texture = false;
    SwapchainHandle active_swapchain;
    const auto inspectAttachment = [&](TextureViewHandle view, const char* label) {
        const auto* glView = m_device.getTextureView(view);
        const auto* glTexture = glView ? m_device.getTexture(glView->texture) : nullptr;
        if (glTexture == nullptr) {
            std::printf("OpenGL render pass begin failed: invalid %s attachment.\n", label);
            return false;
        }

        if (glTexture->is_swapchain_backbuffer) {
            if (!glTexture->swapchain) {
                std::printf("OpenGL render pass begin failed: invalid swapchain %s attachment.\n", label);
                return false;
            }

            if (!active_swapchain) {
                active_swapchain = glTexture->swapchain;
            } else if (active_swapchain != glTexture->swapchain) {
                std::printf("OpenGL render pass begin failed: cannot mix multiple swapchains.\n");
                return false;
            }

            uses_swapchain = true;
        } else {
            uses_offscreen_texture = true;
        }

        return true;
    };

    for (const auto& color : info.color_attachments) {
        if (!inspectAttachment(color.view, "color")) {
            return;
        }
    }

    if (info.has_depth_stencil_attachment && !inspectAttachment(info.depth_stencil_attachment.view, "depth stencil")) {
        return;
    }

    if (uses_swapchain && uses_offscreen_texture) {
        std::printf("OpenGL render pass begin failed: cannot mix swapchain and offscreen attachments.\n");
        return;
    }

    if (uses_swapchain && !m_device.makeSwapchainCurrent(active_swapchain)) {
        std::printf("OpenGL render pass begin failed: could not make swapchain current.\n");
        return;
    }

    const GLuint framebuffer = uses_swapchain ? 0 : m_device.getFramebuffer(info);
    if (!uses_swapchain && framebuffer == 0) {
        std::printf("OpenGL render pass begin failed: framebuffer is incomplete.\n");
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    for (const auto& color : info.color_attachments) {
        auto* glView = m_device.getTextureView(color.view);
        auto* glTexture = glView ? m_device.getTexture(glView->texture) : nullptr;
        if (glTexture != nullptr) {
            if (!textureStateCompatible(*glTexture, ResourceState::ColorAttachment)) {
                std::printf(
                    "OpenGL render pass begin failed: color attachment texture was not created for rendering.\n");
                return;
            }
            transitionTexture(*glTexture, ResourceState::ColorAttachment);
        }
    }

    if (info.has_depth_stencil_attachment) {
        auto* glView = m_device.getTextureView(info.depth_stencil_attachment.view);
        auto* glTexture = glView ? m_device.getTexture(glView->texture) : nullptr;
        if (glTexture != nullptr) {
            if (!textureStateCompatible(*glTexture, ResourceState::DepthStencilWrite)) {
                std::printf(
                    "OpenGL render pass begin failed: depth stencil attachment texture was not created for depth "
                    "stencil rendering.\n");
                return;
            }
            transitionTexture(*glTexture, ResourceState::DepthStencilWrite);
        }
    }

    bool encodeSrgb = false;
    for (const auto& color : info.color_attachments) {
        const auto* glView = m_device.getTextureView(color.view);
        encodeSrgb = encodeSrgb || (glView != nullptr && isSRGBFormat(glView->format));
    }
    if (encodeSrgb) {
        glEnable(GL_FRAMEBUFFER_SRGB);
    } else {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }

    if (uses_swapchain) {
        if (info.color_attachments.empty()) {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        } else {
            glDrawBuffer(GL_BACK);
            glReadBuffer(GL_BACK);
        }
    }

    if (info.width > 0 && info.height > 0) {
        glViewport(0, 0, static_cast<GLsizei>(info.width), static_cast<GLsizei>(info.height));
    }
    glDisable(GL_SCISSOR_TEST);

    for (size_t i = 0; i < info.color_attachments.size(); ++i) {
        const auto& color = info.color_attachments[i];
        if (color.load_op == LoadOp::Clear) {
            const float clearColor[] = {
                color.clear_color.r,
                color.clear_color.g,
                color.clear_color.b,
                color.clear_color.a,
            };
            glClearBufferfv(GL_COLOR, static_cast<GLint>(i), clearColor);
        }
    }

    if (info.has_depth_stencil_attachment && info.depth_stencil_attachment.depth_load_op == LoadOp::Clear) {
        GLboolean previousDepthMask = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
        glDepthMask(GL_TRUE);
        const float clearDepth = info.depth_stencil_attachment.clear_depth;
        glClearBufferfv(GL_DEPTH, 0, &clearDepth);
        glDepthMask(previousDepthMask);
    }
}

void OpenGLCommandList::endRenderPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_FRAMEBUFFER_SRGB);
}

void OpenGLCommandList::setPipeline(PipelineHandle pipeline)
{
    auto* glPipeline = m_device.getPipeline(pipeline);
    if (glPipeline == nullptr) {
        return;
    }

    m_current_pipeline = pipeline;
    glUseProgram(glPipeline->program);

    if (glPipeline->type == OpenGLPipelineType::Compute) {
        glBindVertexArray(0);
        return;
    }

    glBindVertexArray(glPipeline->vao);

    if (glPipeline->depth_state.enabled) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(glPipeline->depth_state.write_enabled ? GL_TRUE : GL_FALSE);
        glDepthFunc(toGLCompareOp(glPipeline->depth_state.compare));
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    if (glPipeline->raster_state.cull_mode == CullMode::None) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(toGLCullMode(glPipeline->raster_state.cull_mode));
    }
    glFrontFace(toGLFrontFace(glPipeline->raster_state.front_face));

    for (uint32_t i = 0; i < static_cast<uint32_t>(glPipeline->render_target_state.color_targets.size()); ++i) {
        const auto& target = glPipeline->render_target_state.color_targets[i];
        glColorMaski(i,
                     colorWriteEnabled(target.write_mask, ColorWriteMask::R),
                     colorWriteEnabled(target.write_mask, ColorWriteMask::G),
                     colorWriteEnabled(target.write_mask, ColorWriteMask::B),
                     colorWriteEnabled(target.write_mask, ColorWriteMask::A));

        if (target.blend.enabled) {
            glEnablei(GL_BLEND, i);
            glBlendFuncSeparatei(i,
                                 toGLBlendFactor(target.blend.src_color),
                                 toGLBlendFactor(target.blend.dst_color),
                                 toGLBlendFactor(target.blend.src_alpha),
                                 toGLBlendFactor(target.blend.dst_alpha));
            glBlendEquationSeparatei(i, toGLBlendOp(target.blend.color_op), toGLBlendOp(target.blend.alpha_op));
        } else {
            glDisablei(GL_BLEND, i);
        }
    }
}

void OpenGLCommandList::setBindGroup(uint32_t set,
                                     BindGroupHandle group,
                                     const uint32_t* dynamic_offsets,
                                     uint32_t dynamic_offset_count)
{
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);
    auto* glGroup = m_device.getBindGroup(group);
    if (glPipeline == nullptr || glGroup == nullptr) {
        return;
    }

    auto* pipelineLayout = m_device.getPipelineLayout(glPipeline->layout);
    if (pipelineLayout == nullptr || set >= pipelineLayout->desc.bind_group_layouts.size() ||
        pipelineLayout->desc.bind_group_layouts[set] != glGroup->layout) {
        return;
    }

    auto* groupLayout = m_device.getBindGroupLayout(glGroup->layout);
    if (groupLayout == nullptr || dynamicOffsetCount(groupLayout->desc) != dynamic_offset_count ||
        (dynamic_offset_count > 0 && dynamic_offsets == nullptr)) {
        return;
    }

    for (const auto& entry : glGroup->entries) {
        const auto binding = flattenedBinding(set, entry.binding);
        const auto* layoutEntry = findLayoutEntry(groupLayout->desc, entry.binding);
        const size_t dynamicOffset = layoutEntry != nullptr && layoutEntry->dynamic_offset
                                         ? dynamicOffsetForBinding(groupLayout->desc, entry.binding, dynamic_offsets)
                                         : 0;

        switch (entry.type) {
            case BindingType::UniformBuffer: {
                auto* glBuffer = m_device.getBuffer(entry.buffer.buffer);
                if (glBuffer == nullptr || !hasUsage(glBuffer->usage, BufferUsage::Uniform)) {
                    break;
                }
                if (!bufferStateUsable(*glBuffer, ResourceState::UniformRead)) {
                    std::printf("OpenGL bind group binding failed: uniform buffer is not in UniformRead state.\n");
                    break;
                }

                const size_t offset = entry.buffer.offset + dynamicOffset;
                if (offset > glBuffer->size || (entry.buffer.size > 0 && entry.buffer.size > glBuffer->size - offset)) {
                    break;
                }

                if (entry.buffer.size > 0) {
                    glBindBufferRange(GL_UNIFORM_BUFFER,
                                      binding,
                                      glBuffer->id,
                                      static_cast<GLintptr>(offset),
                                      static_cast<GLsizeiptr>(entry.buffer.size));
                } else if (dynamicOffset > 0) {
                    glBindBufferRange(GL_UNIFORM_BUFFER,
                                      binding,
                                      glBuffer->id,
                                      static_cast<GLintptr>(offset),
                                      static_cast<GLsizeiptr>(glBuffer->size - offset));
                } else {
                    glBindBufferBase(GL_UNIFORM_BUFFER, binding, glBuffer->id);
                }
                break;
            }
            case BindingType::StorageBuffer: {
                auto* glBuffer = m_device.getBuffer(entry.buffer.buffer);
                if (glBuffer == nullptr || !hasUsage(glBuffer->usage, BufferUsage::Storage)) {
                    break;
                }
                if (!bufferStorageStateUsable(*glBuffer)) {
                    std::printf(
                        "OpenGL bind group binding failed: storage buffer is not in StorageRead or StorageReadWrite "
                        "state.\n");
                    break;
                }

                const size_t offset = entry.buffer.offset + dynamicOffset;
                if (offset > glBuffer->size || (entry.buffer.size > 0 && entry.buffer.size > glBuffer->size - offset)) {
                    break;
                }

                if (entry.buffer.size > 0) {
                    glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                      binding,
                                      glBuffer->id,
                                      static_cast<GLintptr>(offset),
                                      static_cast<GLsizeiptr>(entry.buffer.size));
                } else if (dynamicOffset > 0) {
                    glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                      binding,
                                      glBuffer->id,
                                      static_cast<GLintptr>(offset),
                                      static_cast<GLsizeiptr>(glBuffer->size - offset));
                } else {
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, glBuffer->id);
                }
                break;
            }
            case BindingType::SampledTexture: {
                auto* glView = m_device.getTextureView(entry.texture_view);
                auto* glTexture = glView ? m_device.getTexture(glView->texture) : nullptr;
                if (glTexture != nullptr && !glTexture->is_swapchain_backbuffer) {
                    if (!textureStateUsable(*glTexture, ResourceState::ShaderRead)) {
                        std::printf("OpenGL bind group binding failed: sampled texture is not in ShaderRead state.\n");
                        break;
                    }
                    glBindTextureUnit(binding, glTexture->id);
                }
                break;
            }
            case BindingType::StorageTexture: {
                auto* glView = m_device.getTextureView(entry.texture_view);
                auto* glTexture = glView ? m_device.getTexture(glView->texture) : nullptr;
                if (glTexture != nullptr && !glTexture->is_swapchain_backbuffer &&
                    hasUsage(glTexture->desc.usage, TextureUsage::Storage) && !isDepthFormat(glView->format) &&
                    !isSRGBFormat(glView->format)) {
                    if (!textureStateUsable(*glTexture, ResourceState::StorageReadWrite)) {
                        std::printf(
                            "OpenGL bind group binding failed: storage texture is not in StorageReadWrite state.\n");
                        break;
                    }
                    const GLboolean layered = glView->array_layer_count > 1 ? GL_TRUE : GL_FALSE;
                    const GLint layer = layered == GL_TRUE ? 0 : static_cast<GLint>(glView->base_array_layer);
                    glBindImageTexture(binding,
                                       glTexture->id,
                                       static_cast<GLint>(glView->base_mip_level),
                                       layered,
                                       layer,
                                       GL_READ_WRITE,
                                       toGLTextureInternalFormat(glView->format));
                }
                break;
            }
            case BindingType::Sampler: {
                auto* glSampler = m_device.getSampler(entry.sampler);
                if (glSampler != nullptr) {
                    glBindSampler(binding, glSampler->id);
                }
                break;
            }
            case BindingType::CombinedImageSampler: {
                auto* glView = m_device.getTextureView(entry.texture_view);
                auto* glTexture = glView ? m_device.getTexture(glView->texture) : nullptr;
                auto* glSampler = m_device.getSampler(entry.sampler);
                if (glTexture != nullptr && !glTexture->is_swapchain_backbuffer) {
                    if (!textureStateUsable(*glTexture, ResourceState::ShaderRead)) {
                        std::printf(
                            "OpenGL bind group binding failed: combined image texture is not in ShaderRead state.\n");
                        break;
                    }
                    glBindTextureUnit(binding, glTexture->id);
                }
                if (glSampler != nullptr) {
                    glBindSampler(binding, glSampler->id);
                }
                break;
            }
        }
    }
}

void OpenGLCommandList::setVertexBuffer(uint32_t slot, BufferHandle buffer, size_t offset)
{
    auto* glBuffer = m_device.getBuffer(buffer);
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);

    if (glBuffer == nullptr || glPipeline == nullptr || glPipeline->type != OpenGLPipelineType::Graphics ||
        !hasUsage(glBuffer->usage, BufferUsage::Vertex)) {
        return;
    }
    if (!bufferStateUsable(*glBuffer, ResourceState::VertexBuffer)) {
        std::printf("OpenGL vertex buffer binding failed: buffer is not in VertexBuffer state.\n");
        return;
    }

    for (const auto& bufferLayout : glPipeline->vertex_input.buffers) {
        if (bufferLayout.binding == slot) {
            glVertexArrayVertexBuffer(
                glPipeline->vao, slot, glBuffer->id, static_cast<GLintptr>(offset), bufferLayout.stride);
            return;
        }
    }
}

void OpenGLCommandList::setIndexBuffer(BufferHandle buffer, IndexFormat format, size_t offset)
{
    auto* glBuffer = m_device.getBuffer(buffer);
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);

    if (glBuffer == nullptr || glPipeline == nullptr || glPipeline->type != OpenGLPipelineType::Graphics ||
        !hasUsage(glBuffer->usage, BufferUsage::Index)) {
        return;
    }
    if (!bufferStateUsable(*glBuffer, ResourceState::IndexBuffer)) {
        std::printf("OpenGL index buffer binding failed: buffer is not in IndexBuffer state.\n");
        return;
    }

    m_current_index_buffer = buffer;
    m_current_index_format = format;
    m_current_index_buffer_offset = offset;
    glVertexArrayElementBuffer(glPipeline->vao, glBuffer->id);
}

void OpenGLCommandList::setViewport(uint32_t first, const Viewport* viewports, uint32_t count)
{
    if (viewports == nullptr || count == 0) {
        return;
    }

    std::vector<GLfloat> values;
    values.reserve(static_cast<size_t>(count) * 4);
    std::vector<GLdouble> depthRanges;
    depthRanges.reserve(static_cast<size_t>(count) * 2);

    for (uint32_t i = 0; i < count; ++i) {
        const auto& viewport = viewports[i];
        values.push_back(viewport.x);
        values.push_back(viewport.y);
        values.push_back(viewport.width);
        values.push_back(viewport.height);
        depthRanges.push_back(viewport.min_depth);
        depthRanges.push_back(viewport.max_depth);
    }

    glViewportArrayv(first, static_cast<GLsizei>(count), values.data());
    glDepthRangeArrayv(first, static_cast<GLsizei>(count), depthRanges.data());
}

void OpenGLCommandList::setScissor(uint32_t first, const ScissorRect* scissors, uint32_t count)
{
    if (scissors == nullptr || count == 0) {
        return;
    }

    std::vector<GLint> values;
    values.reserve(static_cast<size_t>(count) * 4);

    for (uint32_t i = 0; i < count; ++i) {
        const auto& scissor = scissors[i];
        values.push_back(scissor.x);
        values.push_back(scissor.y);
        values.push_back(static_cast<GLint>(scissor.width));
        values.push_back(static_cast<GLint>(scissor.height));
    }

    glEnable(GL_SCISSOR_TEST);
    glScissorArrayv(first, static_cast<GLsizei>(count), values.data());
}

void OpenGLCommandList::pushConstants(ShaderStageFlags stages, uint32_t offset, uint32_t size, const void* data)
{
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);
    if (glPipeline == nullptr || data == nullptr) {
        return;
    }

    const auto* layout = m_device.getPipelineLayout(glPipeline->layout);
    if (layout == nullptr || !pushConstantWriteCovered(layout->desc, stages, offset, size)) {
        return;
    }

    uploadPushConstantUniforms(glPipeline->program, offset, size, data);
}

void OpenGLCommandList::transition(const BufferTransition* transitions, uint32_t count)
{
    if (transitions == nullptr || count == 0) {
        return;
    }

    GLbitfield bits = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& transition = transitions[i];
        auto* glBuffer = m_device.getBuffer(transition.buffer);
        if (glBuffer == nullptr || !bufferStateCompatible(*glBuffer, transition.state)) {
            continue;
        }

        bits |= bufferBarrierBits(glBuffer->state, transition.state);
        glBuffer->state = transition.state;
    }

    if (bits != 0) {
        glMemoryBarrier(bits);
    }
}

void OpenGLCommandList::transition(const TextureTransition* transitions, uint32_t count)
{
    if (transitions == nullptr || count == 0) {
        return;
    }

    GLbitfield bits = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& transition = transitions[i];
        auto* glTexture = m_device.getTexture(transition.texture);
        if (glTexture == nullptr || !textureStateCompatible(*glTexture, transition.state)) {
            continue;
        }

        bits |= textureBarrierBits(glTexture->state, transition.state);
        glTexture->state = transition.state;
    }

    if (bits != 0) {
        glMemoryBarrier(bits);
    }
}

void OpenGLCommandList::copyBufferToBuffer(BufferHandle src,
                                           BufferHandle dst,
                                           const BufferCopyRegion* regions,
                                           uint32_t count)
{
    if (regions == nullptr || count == 0) {
        return;
    }

    auto* srcBuffer = m_device.getBuffer(src);
    auto* dstBuffer = m_device.getBuffer(dst);
    if (srcBuffer == nullptr || dstBuffer == nullptr) {
        return;
    }
    if (!hasUsage(srcBuffer->usage, BufferUsage::CopySrc) || srcBuffer->state != ResourceState::CopySrc) {
        std::printf("OpenGL buffer copy failed: source buffer is not in CopySrc state or usage.\n");
        return;
    }
    if (!hasUsage(dstBuffer->usage, BufferUsage::CopyDst) || dstBuffer->state != ResourceState::CopyDst) {
        std::printf("OpenGL buffer copy failed: destination buffer is not in CopyDst state or usage.\n");
        return;
    }

    bool copied = false;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& region = regions[i];
        if (region.size == 0 || region.src_offset > srcBuffer->size ||
            region.size > srcBuffer->size - region.src_offset || region.dst_offset > dstBuffer->size ||
            region.size > dstBuffer->size - region.dst_offset) {
            continue;
        }

        glCopyNamedBufferSubData(srcBuffer->id,
                                 dstBuffer->id,
                                 static_cast<GLintptr>(region.src_offset),
                                 static_cast<GLintptr>(region.dst_offset),
                                 static_cast<GLsizeiptr>(region.size));
        copied = true;
    }

    if (copied) {
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    }
}

void OpenGLCommandList::copyBufferToTexture(BufferHandle src,
                                            TextureHandle dst,
                                            const BufferTextureCopyRegion* regions,
                                            uint32_t count)
{
    if (regions == nullptr || count == 0) {
        return;
    }

    auto* srcBuffer = m_device.getBuffer(src);
    auto* dstTexture = m_device.getTexture(dst);
    if (srcBuffer == nullptr || dstTexture == nullptr || dstTexture->is_swapchain_backbuffer ||
        isDepthFormat(dstTexture->desc.format)) {
        return;
    }
    if (!hasUsage(srcBuffer->usage, BufferUsage::CopySrc) || srcBuffer->state != ResourceState::CopySrc) {
        std::printf("OpenGL texture copy failed: source buffer is not in CopySrc state or usage.\n");
        return;
    }
    if (!hasUsage(dstTexture->desc.usage, TextureUsage::CopyDst) || dstTexture->state != ResourceState::CopyDst) {
        std::printf("OpenGL texture copy failed: destination texture is not in CopyDst state or usage.\n");
        return;
    }

    GLint previousBuffer = 0;
    GLint previousAlignment = 4;
    GLint previousRowLength = 0;
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &previousBuffer);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &previousRowLength);

    const auto bytesPerPixel = textureFormatBytesPerPixel(dstTexture->desc.format);
    bool copied = false;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, srcBuffer->id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (uint32_t i = 0; i < count; ++i) {
        const auto& region = regions[i];
        if (region.texture_width == 0 || region.texture_height == 0 ||
            region.mip_level >= dstTexture->desc.mip_levels || region.array_layer >= dstTexture->desc.array_layers ||
            region.buffer_offset > srcBuffer->size) {
            continue;
        }

        const uint32_t mipWidth = textureMipDimension(dstTexture->desc.width, region.mip_level);
        const uint32_t mipHeight = textureMipDimension(dstTexture->desc.height, region.mip_level);
        if (region.texture_x > mipWidth || region.texture_y > mipHeight ||
            region.texture_width > mipWidth - region.texture_x ||
            region.texture_height > mipHeight - region.texture_y) {
            continue;
        }

        const size_t rowPitch = region.buffer_row_pitch == 0 ? static_cast<size_t>(region.texture_width) * bytesPerPixel
                                                             : region.buffer_row_pitch;
        if (rowPitch % bytesPerPixel != 0 || rowPitch < static_cast<size_t>(region.texture_width) * bytesPerPixel ||
            rowPitch > (srcBuffer->size - region.buffer_offset) / region.texture_height) {
            continue;
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(rowPitch / bytesPerPixel));
        const auto* bufferOffset = reinterpret_cast<const void*>(region.buffer_offset);
        if (dstTexture->desc.dimension == TextureDimension::TextureCube) {
            glTextureSubImage3D(dstTexture->id,
                                static_cast<GLint>(region.mip_level),
                                static_cast<GLint>(region.texture_x),
                                static_cast<GLint>(region.texture_y),
                                static_cast<GLint>(region.array_layer),
                                static_cast<GLsizei>(region.texture_width),
                                static_cast<GLsizei>(region.texture_height),
                                1,
                                toGLTextureUploadFormat(dstTexture->desc.format),
                                toGLTextureUploadType(dstTexture->desc.format),
                                bufferOffset);
        } else {
            glTextureSubImage2D(dstTexture->id,
                                static_cast<GLint>(region.mip_level),
                                static_cast<GLint>(region.texture_x),
                                static_cast<GLint>(region.texture_y),
                                static_cast<GLsizei>(region.texture_width),
                                static_cast<GLsizei>(region.texture_height),
                                toGLTextureUploadFormat(dstTexture->desc.format),
                                toGLTextureUploadType(dstTexture->desc.format),
                                bufferOffset);
        }
        copied = true;
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, previousRowLength);
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(previousBuffer));

    if (copied) {
        glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    }
}

void OpenGLCommandList::generateMipmaps(TextureHandle texture)
{
    auto* glTexture = m_device.getTexture(texture);
    if (glTexture == nullptr || glTexture->is_swapchain_backbuffer || isDepthFormat(glTexture->desc.format) ||
        glTexture->desc.mip_levels <= 1) {
        return;
    }

    if (!hasUsage(glTexture->desc.usage, TextureUsage::CopySrc | TextureUsage::CopyDst)) {
        std::printf("OpenGL mipmap generation failed: texture was not created with CopySrc and CopyDst usage.\n");
        return;
    }

    if (glTexture->state != ResourceState::CopyDst) {
        std::printf("OpenGL mipmap generation failed: texture is not in CopyDst state.\n");
        return;
    }

    glGenerateTextureMipmap(glTexture->id);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void OpenGLCommandList::draw(uint32_t vertex_count, uint32_t first_vertex)
{
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);
    if (glPipeline == nullptr || glPipeline->type != OpenGLPipelineType::Graphics) {
        return;
    }

    glDrawArrays(glPipeline->topology, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
}

void OpenGLCommandList::drawIndexed(uint32_t index_count, uint32_t first_index, int32_t vertex_offset)
{
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);
    auto* glIndexBuffer = m_device.getBuffer(m_current_index_buffer);
    if (glPipeline == nullptr || glPipeline->type != OpenGLPipelineType::Graphics || glIndexBuffer == nullptr ||
        !hasUsage(glIndexBuffer->usage, BufferUsage::Index)) {
        return;
    }

    const auto offset =
        m_current_index_buffer_offset + static_cast<uintptr_t>(first_index) * indexFormatSize(m_current_index_format);
    glDrawElementsBaseVertex(glPipeline->topology,
                             static_cast<GLsizei>(index_count),
                             toGLIndexFormat(m_current_index_format),
                             reinterpret_cast<const void*>(offset),
                             static_cast<GLint>(vertex_offset));
}

void OpenGLCommandList::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);
    if (glPipeline == nullptr || glPipeline->type != OpenGLPipelineType::Compute || group_count_x == 0 ||
        group_count_y == 0 || group_count_z == 0) {
        return;
    }

    glDispatchCompute(group_count_x, group_count_y, group_count_z);
}

} // namespace lunalite::rhi
