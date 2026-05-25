#include "command_list.h"
#include "device.h"
#include "gl_convert.h"

#include <cstdio>
#include <cstdint>

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
} // namespace

void OpenGLCommandList::begin()
{
    m_current_pipeline = 0;
    m_current_index_buffer = 0;
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
    const auto inspectAttachment = [&](TextureViewHandle view, const char* label) {
        const auto* glView = m_device.getTextureView(view);
        const auto* glTexture = glView ? m_device.getTexture(glView->texture) : nullptr;
        if (glTexture == nullptr) {
            std::printf("OpenGL render pass begin failed: invalid %s attachment.\n", label);
            return false;
        }

        if (glTexture->is_swapchain_backbuffer) {
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

    const GLuint framebuffer = uses_swapchain ? 0 : m_device.getFramebuffer(info);
    if (!uses_swapchain && framebuffer == 0) {
        std::printf("OpenGL render pass begin failed: framebuffer is incomplete.\n");
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

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
        const float clearDepth = info.depth_stencil_attachment.clear_depth;
        glClearBufferfv(GL_DEPTH, 0, &clearDepth);
    }
}

void OpenGLCommandList::endRenderPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLCommandList::setPipeline(PipelineHandle pipeline)
{
    auto* glPipeline = m_device.getPipeline(pipeline);
    if (glPipeline == nullptr) {
        return;
    }

    m_current_pipeline = pipeline;
    glUseProgram(glPipeline->program);
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
            glBlendEquationSeparatei(i,
                                     toGLBlendOp(target.blend.color_op),
                                     toGLBlendOp(target.blend.alpha_op));
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
    if (pipelineLayout == nullptr || set >= pipelineLayout->desc.bind_group_layouts.size()
        || pipelineLayout->desc.bind_group_layouts[set] != glGroup->layout) {
        return;
    }

    auto* groupLayout = m_device.getBindGroupLayout(glGroup->layout);
    if (groupLayout == nullptr || dynamicOffsetCount(groupLayout->desc) != dynamic_offset_count
        || (dynamic_offset_count > 0 && dynamic_offsets == nullptr)) {
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
                    glBindTextureUnit(binding, glTexture->id);
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

    if (glBuffer == nullptr || glPipeline == nullptr || !hasUsage(glBuffer->usage, BufferUsage::Vertex)) {
        return;
    }

    for (const auto& bufferLayout : glPipeline->vertex_input.buffers) {
        if (bufferLayout.binding == slot) {
            glVertexArrayVertexBuffer(glPipeline->vao,
                                      slot,
                                      glBuffer->id,
                                      static_cast<GLintptr>(offset),
                                      bufferLayout.stride);
            return;
        }
    }
}

void OpenGLCommandList::setIndexBuffer(BufferHandle buffer, IndexFormat format, size_t offset)
{
    auto* glBuffer = m_device.getBuffer(buffer);
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);

    if (glBuffer == nullptr || glPipeline == nullptr || !hasUsage(glBuffer->usage, BufferUsage::Index)) {
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

void OpenGLCommandList::resourceBarrier(const TextureBarrier* barriers, uint32_t count)
{
    if (barriers == nullptr || count == 0) {
        return;
    }

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
}

void OpenGLCommandList::draw(uint32_t vertex_count, uint32_t first_vertex)
{
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);
    if (glPipeline == nullptr) {
        return;
    }

    glDrawArrays(glPipeline->topology, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
}

void OpenGLCommandList::drawIndexed(uint32_t index_count, uint32_t first_index, int32_t vertex_offset)
{
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);
    auto* glIndexBuffer = m_device.getBuffer(m_current_index_buffer);
    if (glPipeline == nullptr || glIndexBuffer == nullptr || !hasUsage(glIndexBuffer->usage, BufferUsage::Index)) {
        return;
    }

    const auto offset = m_current_index_buffer_offset + static_cast<uintptr_t>(first_index) * indexFormatSize(m_current_index_format);
    glDrawElementsBaseVertex(glPipeline->topology,
                             static_cast<GLsizei>(index_count),
                             toGLIndexFormat(m_current_index_format),
                             reinterpret_cast<const void*>(offset),
                             static_cast<GLint>(vertex_offset));
}

} // namespace lunalite::rhi
