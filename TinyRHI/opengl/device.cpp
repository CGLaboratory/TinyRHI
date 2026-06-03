#include "command_list.h"
#include "device.h"
#include "gl_convert.h"
#include "swapchain.h"

#include <utility>

namespace lunalite::rhi {

namespace {
bool hasUsage(BufferUsage usage, BufferUsage required)
{
    return (usage & required) == required;
}

bool hasUsage(TextureUsage usage, TextureUsage required)
{
    return (usage & required) == required;
}

bool isBufferBinding(BindingType type)
{
    return type == BindingType::UniformBuffer || type == BindingType::StorageBuffer;
}

const BindGroupLayoutEntry* findLayoutEntry(const BindGroupLayoutDesc& layout, uint32_t binding)
{
    for (const auto& layoutEntry : layout.entries) {
        if (layoutEntry.binding == binding) {
            return &layoutEntry;
        }
    }

    return nullptr;
}

bool bindGroupLayoutDescValid(const BindGroupLayoutDesc& desc)
{
    for (size_t i = 0; i < desc.entries.size(); ++i) {
        const auto& entry = desc.entries[i];
        if (entry.count != 1) {
            return false;
        }

        if (entry.dynamic_offset && !isBufferBinding(entry.type)) {
            return false;
        }

        for (size_t j = i + 1; j < desc.entries.size(); ++j) {
            if (entry.binding == desc.entries[j].binding) {
                return false;
            }
        }
    }

    return true;
}

bool bindGroupDescMatchesLayout(const BindGroupLayoutDesc& layout, const BindGroupDesc& desc)
{
    if (desc.entries.size() != layout.entries.size()) {
        return false;
    }

    for (const auto& entry : desc.entries) {
        const auto* layoutEntry = findLayoutEntry(layout, entry.binding);
        if (layoutEntry == nullptr || layoutEntry->type != entry.type) {
            return false;
        }
    }

    for (size_t i = 0; i < desc.entries.size(); ++i) {
        for (size_t j = i + 1; j < desc.entries.size(); ++j) {
            if (desc.entries[i].binding == desc.entries[j].binding) {
                return false;
            }
        }
    }

    return true;
}

bool bufferBindingValid(const OpenGLBuffer& buffer, const BufferBinding& binding)
{
    if (binding.offset > buffer.size) {
        return false;
    }

    return binding.size == 0 || binding.size <= buffer.size - binding.offset;
}

bool bindGroupEntryResourcesValid(OpenGLDevice& device, const BindGroupEntry& entry)
{
    switch (entry.type) {
        case BindingType::UniformBuffer: {
            const auto* buffer = device.getBuffer(entry.buffer.buffer);
            return buffer != nullptr && hasUsage(buffer->usage, BufferUsage::Uniform) &&
                   bufferBindingValid(*buffer, entry.buffer);
        }
        case BindingType::StorageBuffer: {
            const auto* buffer = device.getBuffer(entry.buffer.buffer);
            return buffer != nullptr && hasUsage(buffer->usage, BufferUsage::Storage) &&
                   bufferBindingValid(*buffer, entry.buffer);
        }
        case BindingType::SampledTexture:
            return device.getTextureView(entry.texture_view) != nullptr;
        case BindingType::StorageTexture: {
            const auto* view = device.getTextureView(entry.texture_view);
            const auto* texture = view != nullptr ? device.getTexture(view->texture) : nullptr;
            return texture != nullptr && hasUsage(texture->desc.usage, TextureUsage::Storage) &&
                   !isDepthFormat(view->format) && !isSRGBFormat(view->format);
        }
        case BindingType::Sampler:
            return device.getSampler(entry.sampler) != nullptr;
        case BindingType::CombinedImageSampler:
            return device.getTextureView(entry.texture_view) != nullptr && device.getSampler(entry.sampler) != nullptr;
    }

    return false;
}

bool pushConstantRangesValid(const std::vector<PushConstantRange>& ranges)
{
    for (size_t i = 0; i < ranges.size(); ++i) {
        const auto& range = ranges[i];
        if (range.size == 0 || range.stages == 0) {
            return false;
        }

        const uint32_t rangeEnd = range.offset + range.size;
        if (rangeEnd < range.offset) {
            return false;
        }

        for (size_t j = i + 1; j < ranges.size(); ++j) {
            const auto& other = ranges[j];
            const uint32_t otherEnd = other.offset + other.size;
            if (otherEnd < other.offset) {
                return false;
            }

            if ((range.stages & other.stages) != 0 && range.offset < otherEnd && other.offset < rangeEnd) {
                return false;
            }
        }
    }

    return true;
}

bool attachFramebufferTextureView(GLuint framebuffer,
                                  GLenum attachment,
                                  const OpenGLTexture& texture,
                                  const OpenGLTextureView& view)
{
    if (texture.desc.dimension == TextureDimension::TextureCube) {
        if (view.view_dimension != TextureViewDimension::Texture2D || view.array_layer_count != 1) {
            return false;
        }

        glNamedFramebufferTextureLayer(framebuffer,
                                       attachment,
                                       texture.id,
                                       static_cast<GLint>(view.base_mip_level),
                                       static_cast<GLint>(view.base_array_layer));
        return true;
    }

    if (view.view_dimension != TextureViewDimension::Texture2D) {
        return false;
    }

    glNamedFramebufferTexture(framebuffer, attachment, texture.id, view.base_mip_level);
    return true;
}
} // namespace

OpenGLDevice::OpenGLDevice(SurfaceResolver surface_resolver)
    : m_surface_resolver(std::move(surface_resolver)),
      m_command_list(std::make_unique<OpenGLCommandList>(*this))
{}

OpenGLDevice::~OpenGLDevice()
{
    if (m_native_context.context != nullptr) {
        makeAnySwapchainCurrent();
    }

    m_command_list.reset();

    for (auto& pipeline : m_pipelines) {
        if (pipeline.vao != 0) {
            glDeleteVertexArrays(1, &pipeline.vao);
        }
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

    releaseContext();
    m_swapchains.clear();
}

CommandList& OpenGLDevice::getCommandList()
{
    return *m_command_list;
}

bool OpenGLDevice::beginFrame(SwapchainHandle swapchain, SwapchainFrame& frame)
{
    auto* glSwapchain = getOpenGLSwapchain(swapchain);
    if (glSwapchain == nullptr) {
        return false;
    }

    if (!makeSwapchainCurrent(swapchain)) {
        return false;
    }

    frame = SwapchainFrame{
        .swapchain = swapchain,
        .color_view = glSwapchain->getCurrentColorTextureView(),
        .depth_stencil_view = glSwapchain->getDepthStencilTextureView(),
        .width = glSwapchain->getWidth(),
        .height = glSwapchain->getHeight(),
    };
    m_active_frame_swapchain = swapchain;
    return true;
}

void OpenGLDevice::submit(const SwapchainFrame* frame)
{
    (void) frame;
    glFlush();
}

void OpenGLDevice::present(const SwapchainFrame& frame)
{
    auto* glSwapchain = getOpenGLSwapchain(frame.swapchain);
    if (glSwapchain == nullptr) {
        return;
    }

    glSwapchain->present();
    if (m_active_frame_swapchain == frame.swapchain) {
        m_active_frame_swapchain = {};
    }
}

Surface* OpenGLDevice::getSurface(SurfaceHandle handle)
{
    return m_surface_resolver ? m_surface_resolver(handle) : nullptr;
}

SamplerHandle OpenGLDevice::createSampler(const SamplerDesc& desc)
{
    GLuint sampler = 0;
    glCreateSamplers(1, &sampler);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, toGLMinFilter(desc.min_filter, desc.mip_filter));
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, toGLFilterMode(desc.mag_filter));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, toGLAddressMode(desc.address_u));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, toGLAddressMode(desc.address_v));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, toGLAddressMode(desc.address_w));

    m_samplers.push_back(OpenGLSampler{.id = sampler, .desc = desc});
    return makeHandle<SamplerHandle>(m_samplers.size() - 1);
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
    if (!bindGroupLayoutDescValid(desc)) {
        return {};
    }

    m_bind_group_layouts.push_back(OpenGLBindGroupLayout{.desc = desc, .valid = true});
    return makeHandle<BindGroupLayoutHandle>(m_bind_group_layouts.size() - 1);
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
        return {};
    }

    if (!bindGroupDescMatchesLayout(layout->desc, desc)) {
        return {};
    }

    for (const auto& entry : desc.entries) {
        if (!bindGroupEntryResourcesValid(*this, entry)) {
            return {};
        }
    }

    m_bind_groups.push_back(OpenGLBindGroup{.layout = desc.layout, .entries = desc.entries});
    return makeHandle<BindGroupHandle>(m_bind_groups.size() - 1);
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
        if (!bindGroupEntryResourcesValid(*this, entry)) {
            return;
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

    glGroup->layout = {};
    glGroup->entries.clear();
}

PipelineLayoutHandle OpenGLDevice::createPipelineLayout(const PipelineLayoutDesc& desc)
{
    for (const auto layout : desc.bind_group_layouts) {
        if (getBindGroupLayout(layout) == nullptr) {
            return {};
        }
    }

    if (!pushConstantRangesValid(desc.push_constants)) {
        return {};
    }

    m_pipeline_layouts.push_back(OpenGLPipelineLayout{.desc = desc, .valid = true});
    return makeHandle<PipelineLayoutHandle>(m_pipeline_layouts.size() - 1);
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
    if (!handle || handle.value > m_buffers.size()) {
        return nullptr;
    }

    auto& buffer = m_buffers[handleIndex(handle)];
    if (buffer.id == 0) {
        return nullptr;
    }

    return &buffer;
}

OpenGLTexture* OpenGLDevice::getTexture(TextureHandle handle)
{
    if (!handle || handle.value > m_textures.size()) {
        return nullptr;
    }

    auto& texture = m_textures[handleIndex(handle)];
    if (texture.id == 0 && !texture.is_swapchain_backbuffer) {
        return nullptr;
    }

    return &texture;
}

OpenGLTextureView* OpenGLDevice::getTextureView(TextureViewHandle handle)
{
    if (!handle || handle.value > m_texture_views.size()) {
        return nullptr;
    }

    auto& view = m_texture_views[handleIndex(handle)];
    if (!view.texture) {
        return nullptr;
    }

    return &view;
}

OpenGLSampler* OpenGLDevice::getSampler(SamplerHandle handle)
{
    if (!handle || handle.value > m_samplers.size()) {
        return nullptr;
    }

    auto& sampler = m_samplers[handleIndex(handle)];
    if (sampler.id == 0) {
        return nullptr;
    }

    return &sampler;
}

OpenGLBindGroupLayout* OpenGLDevice::getBindGroupLayout(BindGroupLayoutHandle handle)
{
    if (!handle || handle.value > m_bind_group_layouts.size()) {
        return nullptr;
    }

    auto& layout = m_bind_group_layouts[handleIndex(handle)];
    if (!layout.valid) {
        return nullptr;
    }

    return &layout;
}

OpenGLBindGroup* OpenGLDevice::getBindGroup(BindGroupHandle handle)
{
    if (!handle || handle.value > m_bind_groups.size()) {
        return nullptr;
    }

    auto& group = m_bind_groups[handleIndex(handle)];
    if (!group.layout) {
        return nullptr;
    }

    return &group;
}

OpenGLPipelineLayout* OpenGLDevice::getPipelineLayout(PipelineLayoutHandle handle)
{
    if (!handle || handle.value > m_pipeline_layouts.size()) {
        return nullptr;
    }

    auto& layout = m_pipeline_layouts[handleIndex(handle)];
    if (!layout.valid) {
        return nullptr;
    }

    return &layout;
}

OpenGLShader* OpenGLDevice::getShader(ShaderHandle handle)
{
    if (!handle || handle.value > m_shaders.size()) {
        return nullptr;
    }

    auto& shader = m_shaders[handleIndex(handle)];
    if (shader.id == 0) {
        return nullptr;
    }

    return &shader;
}

OpenGLPipeline* OpenGLDevice::getPipeline(PipelineHandle handle)
{
    if (!handle || handle.value > m_pipelines.size()) {
        return nullptr;
    }

    auto& pipeline = m_pipelines[handleIndex(handle)];
    if (pipeline.program == 0 || (pipeline.type == OpenGLPipelineType::Graphics && pipeline.vao == 0)) {
        return nullptr;
    }

    return &pipeline;
}

GLuint OpenGLDevice::getFramebuffer(const RenderPassBeginInfo& info)
{
    for (const auto& framebuffer : m_framebuffers) {
        if (framebuffer.width != info.width || framebuffer.height != info.height ||
            framebuffer.has_depth_stencil != info.has_depth_stencil_attachment ||
            framebuffer.color_views.size() != info.color_attachments.size()) {
            continue;
        }

        bool matches = framebuffer.depth_stencil_view ==
                       (info.has_depth_stencil_attachment ? info.depth_stencil_attachment.view : TextureViewHandle{});
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
        if (!attachFramebufferTextureView(framebuffer, attachment, *colorTexture, *colorView)) {
            glDeleteFramebuffers(1, &framebuffer);
            return 0;
        }
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

    TextureViewHandle depthStencilViewHandle;
    if (info.has_depth_stencil_attachment) {
        const auto* depthView = getTextureView(info.depth_stencil_attachment.view);
        const auto* depthTexture = depthView ? getTexture(depthView->texture) : nullptr;
        if (depthTexture == nullptr || depthTexture->is_swapchain_backbuffer) {
            glDeleteFramebuffers(1, &framebuffer);
            return 0;
        }

        if (!attachFramebufferTextureView(framebuffer, toGLAttachment(depthView->format), *depthTexture, *depthView)) {
            glDeleteFramebuffers(1, &framebuffer);
            return 0;
        }
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
