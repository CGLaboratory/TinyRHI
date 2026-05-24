#include "device.h"
#include "command_list.h"
#include "gl_convert.h"

namespace lunalite::rhi {

namespace {
bool bindGroupEntryMatchesLayout(const BindGroupLayoutDesc& layout, const BindGroupEntry& entry)
{
    for (const auto& layoutEntry : layout.entries) {
        if (layoutEntry.binding == entry.binding) {
            return layoutEntry.type == entry.type;
        }
    }

    return false;
}

bool bindGroupDescMatchesLayout(const BindGroupLayoutDesc& layout, const BindGroupDesc& desc)
{
    for (const auto& entry : desc.entries) {
        if (!bindGroupEntryMatchesLayout(layout, entry)) {
            return false;
        }
    }

    return true;
}
} // namespace

OpenGLDevice::OpenGLDevice()
    : m_command_list(std::make_unique<OpenGLCommandList>(*this))
{}

OpenGLDevice::~OpenGLDevice()
{
    m_command_list.reset();

    for (auto& pipeline : m_pipelines) {
        glDeleteVertexArrays(1, &pipeline.vao);
        glDeleteProgram(pipeline.program);
    }

    for (auto& framebuffer : m_framebuffers) {
        glDeleteFramebuffers(1, &framebuffer.id);
    }

    for (auto& shader : m_shaders) {
        glDeleteShader(shader.id);
    }

    for (auto& sampler : m_samplers) {
        glDeleteSamplers(1, &sampler.id);
    }

    for (auto& buffer : m_buffers) {
        glDeleteBuffers(1, &buffer.id);
    }

    for (auto& texture : m_textures) {
        if (texture.id != 0 && !texture.is_swapchain_backbuffer) {
            glDeleteTextures(1, &texture.id);
        }
    }
}

CommandList& OpenGLDevice::getCommandList()
{
    return *m_command_list;
}

SamplerHandle OpenGLDevice::createSampler(const SamplerDesc& desc)
{
    GLuint sampler = 0;
    glCreateSamplers(1, &sampler);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, toGLFilterMode(desc.min_filter));
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, toGLFilterMode(desc.mag_filter));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, toGLAddressMode(desc.address_u));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, toGLAddressMode(desc.address_v));

    m_samplers.push_back(OpenGLSampler{.id = sampler, .desc = desc});
    return static_cast<SamplerHandle>(m_samplers.size());
}

void OpenGLDevice::destroySampler(SamplerHandle sampler)
{
    auto* glSampler = getSampler(sampler);
    if (glSampler == nullptr) {
        return;
    }

    glDeleteSamplers(1, &glSampler->id);
    glSampler->id = 0;
}

BindGroupLayoutHandle OpenGLDevice::createBindGroupLayout(const BindGroupLayoutDesc& desc)
{
    m_bind_group_layouts.push_back(OpenGLBindGroupLayout{.desc = desc, .valid = true});
    return static_cast<BindGroupLayoutHandle>(m_bind_group_layouts.size());
}

void OpenGLDevice::destroyBindGroupLayout(BindGroupLayoutHandle layout)
{
    auto* glLayout = getBindGroupLayout(layout);
    if (glLayout == nullptr) {
        return;
    }

    glLayout->valid = false;
    glLayout->desc.entries.clear();
}

BindGroupHandle OpenGLDevice::createBindGroup(const BindGroupDesc& desc)
{
    const auto* layout = getBindGroupLayout(desc.layout);
    if (layout == nullptr) {
        return 0;
    }

    if (!bindGroupDescMatchesLayout(layout->desc, desc)) {
        return 0;
    }

    for (const auto& entry : desc.entries) {
        switch (entry.type) {
            case BindingType::UniformBuffer: {
                const auto* buffer = getBuffer(entry.buffer.buffer);
                if (buffer == nullptr || buffer->type != BufferType::UniformBuffer) {
                    return 0;
                }
                break;
            }
            case BindingType::StorageBuffer: {
                const auto* buffer = getBuffer(entry.buffer.buffer);
                if (buffer == nullptr || buffer->type != BufferType::StorageBuffer) {
                    return 0;
                }
                break;
            }
            case BindingType::SampledTexture:
                if (getTextureView(entry.texture_view) == nullptr) {
                    return 0;
                }
                break;
            case BindingType::Sampler:
                if (getSampler(entry.sampler) == nullptr) {
                    return 0;
                }
                break;
            case BindingType::CombinedImageSampler:
                if (getTextureView(entry.texture_view) == nullptr || getSampler(entry.sampler) == nullptr) {
                    return 0;
                }
                break;
        }
    }

    m_bind_groups.push_back(OpenGLBindGroup{.layout = desc.layout, .entries = desc.entries});
    return static_cast<BindGroupHandle>(m_bind_groups.size());
}

void OpenGLDevice::updateBindGroup(BindGroupHandle group, const BindGroupDesc& desc)
{
    auto* glGroup = getBindGroup(group);
    const auto* layout = getBindGroupLayout(desc.layout);
    if (glGroup == nullptr || layout == nullptr || glGroup->layout != desc.layout) {
        return;
    }

    if (!bindGroupDescMatchesLayout(layout->desc, desc)) {
        return;
    }

    for (const auto& entry : desc.entries) {
        switch (entry.type) {
            case BindingType::UniformBuffer: {
                const auto* buffer = getBuffer(entry.buffer.buffer);
                if (buffer == nullptr || buffer->type != BufferType::UniformBuffer) {
                    return;
                }
                break;
            }
            case BindingType::StorageBuffer: {
                const auto* buffer = getBuffer(entry.buffer.buffer);
                if (buffer == nullptr || buffer->type != BufferType::StorageBuffer) {
                    return;
                }
                break;
            }
            case BindingType::SampledTexture:
                if (getTextureView(entry.texture_view) == nullptr) {
                    return;
                }
                break;
            case BindingType::Sampler:
                if (getSampler(entry.sampler) == nullptr) {
                    return;
                }
                break;
            case BindingType::CombinedImageSampler:
                if (getTextureView(entry.texture_view) == nullptr || getSampler(entry.sampler) == nullptr) {
                    return;
                }
                break;
        }
    }

    glGroup->entries = desc.entries;
}

void OpenGLDevice::destroyBindGroup(BindGroupHandle group)
{
    auto* glGroup = getBindGroup(group);
    if (glGroup == nullptr) {
        return;
    }

    glGroup->layout = 0;
    glGroup->entries.clear();
}

PipelineLayoutHandle OpenGLDevice::createPipelineLayout(const PipelineLayoutDesc& desc)
{
    for (const auto layout : desc.bind_group_layouts) {
        if (getBindGroupLayout(layout) == nullptr) {
            return 0;
        }
    }

    m_pipeline_layouts.push_back(OpenGLPipelineLayout{.desc = desc, .valid = true});
    return static_cast<PipelineLayoutHandle>(m_pipeline_layouts.size());
}

void OpenGLDevice::destroyPipelineLayout(PipelineLayoutHandle layout)
{
    auto* glLayout = getPipelineLayout(layout);
    if (glLayout == nullptr) {
        return;
    }

    glLayout->valid = false;
    glLayout->desc.bind_group_layouts.clear();
    glLayout->desc.push_constants.clear();
}

OpenGLBuffer* OpenGLDevice::getBuffer(BufferHandle handle)
{
    if (handle == 0 || handle > m_buffers.size()) {
        return nullptr;
    }

    auto& buffer = m_buffers[handle - 1];
    if (buffer.id == 0) {
        return nullptr;
    }

    return &m_buffers[handle - 1];
}

OpenGLTexture* OpenGLDevice::getTexture(TextureHandle handle)
{
    if (handle == 0 || handle > m_textures.size()) {
        return nullptr;
    }

    auto& texture = m_textures[handle - 1];
    if (texture.id == 0 && !texture.is_swapchain_backbuffer) {
        return nullptr;
    }

    return &texture;
}

OpenGLTextureView* OpenGLDevice::getTextureView(TextureViewHandle handle)
{
    if (handle == 0 || handle > m_texture_views.size()) {
        return nullptr;
    }

    auto& view = m_texture_views[handle - 1];
    if (view.texture == 0) {
        return nullptr;
    }

    return &view;
}

OpenGLSampler* OpenGLDevice::getSampler(SamplerHandle handle)
{
    if (handle == 0 || handle > m_samplers.size()) {
        return nullptr;
    }

    auto& sampler = m_samplers[handle - 1];
    if (sampler.id == 0) {
        return nullptr;
    }

    return &sampler;
}

OpenGLBindGroupLayout* OpenGLDevice::getBindGroupLayout(BindGroupLayoutHandle handle)
{
    if (handle == 0 || handle > m_bind_group_layouts.size()) {
        return nullptr;
    }

    auto& layout = m_bind_group_layouts[handle - 1];
    if (!layout.valid) {
        return nullptr;
    }

    return &layout;
}

OpenGLBindGroup* OpenGLDevice::getBindGroup(BindGroupHandle handle)
{
    if (handle == 0 || handle > m_bind_groups.size()) {
        return nullptr;
    }

    auto& group = m_bind_groups[handle - 1];
    if (group.layout == 0) {
        return nullptr;
    }

    return &group;
}

OpenGLPipelineLayout* OpenGLDevice::getPipelineLayout(PipelineLayoutHandle handle)
{
    if (handle == 0 || handle > m_pipeline_layouts.size()) {
        return nullptr;
    }

    auto& layout = m_pipeline_layouts[handle - 1];
    if (!layout.valid) {
        return nullptr;
    }

    return &layout;
}

OpenGLShader* OpenGLDevice::getShader(ShaderHandle handle)
{
    if (handle == 0 || handle > m_shaders.size()) {
        return nullptr;
    }

    auto& shader = m_shaders[handle - 1];
    if (shader.id == 0) {
        return nullptr;
    }

    return &shader;
}

OpenGLPipeline* OpenGLDevice::getPipeline(PipelineHandle handle)
{
    if (handle == 0 || handle > m_pipelines.size()) {
        return nullptr;
    }

    auto& pipeline = m_pipelines[handle - 1];
    if (pipeline.program == 0 || pipeline.vao == 0) {
        return nullptr;
    }

    return &pipeline;
}

GLuint OpenGLDevice::getFramebuffer(const RenderPassBeginInfo& info)
{
    for (const auto& framebuffer : m_framebuffers) {
        if (framebuffer.width != info.width || framebuffer.height != info.height
            || framebuffer.has_depth_stencil != info.has_depth_stencil_attachment
            || framebuffer.color_views.size() != info.color_attachments.size()) {
            continue;
        }

        bool matches = framebuffer.depth_stencil_view
                       == (info.has_depth_stencil_attachment ? info.depth_stencil_attachment.view : 0);
        for (size_t i = 0; matches && i < info.color_attachments.size(); ++i) {
            matches = framebuffer.color_views[i] == info.color_attachments[i].view;
        }

        if (matches) {
            return framebuffer.id;
        }
    }

    GLuint framebuffer = 0;
    glCreateFramebuffers(1, &framebuffer);

    std::vector<TextureViewHandle> colorViews;
    colorViews.reserve(info.color_attachments.size());
    std::vector<GLenum> drawBuffers;
    drawBuffers.reserve(info.color_attachments.size());

    for (size_t i = 0; i < info.color_attachments.size(); ++i) {
        const auto* colorView = getTextureView(info.color_attachments[i].view);
        const auto* colorTexture = colorView ? getTexture(colorView->texture) : nullptr;
        if (colorTexture == nullptr || colorTexture->is_swapchain_backbuffer) {
            glDeleteFramebuffers(1, &framebuffer);
            return 0;
        }

        const auto attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
        glNamedFramebufferTexture(framebuffer, attachment, colorTexture->id, colorView->base_mip_level);
        colorViews.push_back(info.color_attachments[i].view);
        drawBuffers.push_back(attachment);
    }

    if (drawBuffers.empty()) {
        glNamedFramebufferDrawBuffer(framebuffer, GL_NONE);
        glNamedFramebufferReadBuffer(framebuffer, GL_NONE);
    } else {
        glNamedFramebufferDrawBuffers(framebuffer, static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
        glNamedFramebufferReadBuffer(framebuffer, drawBuffers.front());
    }

    TextureViewHandle depthStencilViewHandle = 0;
    if (info.has_depth_stencil_attachment) {
        const auto* depthView = getTextureView(info.depth_stencil_attachment.view);
        const auto* depthTexture = depthView ? getTexture(depthView->texture) : nullptr;
        if (depthTexture == nullptr || depthTexture->is_swapchain_backbuffer) {
            glDeleteFramebuffers(1, &framebuffer);
            return 0;
        }

        glNamedFramebufferTexture(framebuffer, toGLAttachment(depthView->format), depthTexture->id, depthView->base_mip_level);
        depthStencilViewHandle = info.depth_stencil_attachment.view;
    }

    if (glCheckNamedFramebufferStatus(framebuffer, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &framebuffer);
        return 0;
    }

    m_framebuffers.push_back(OpenGLFramebuffer{
        .id = framebuffer,
        .color_views = colorViews,
        .has_depth_stencil = info.has_depth_stencil_attachment,
        .depth_stencil_view = depthStencilViewHandle,
        .width = info.width,
        .height = info.height,
    });

    return framebuffer;
}

} // namespace lunalite::rhi
