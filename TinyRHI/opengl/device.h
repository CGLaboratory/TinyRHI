#pragma once
#include "../interface/device.h"
#include "../interface/rhi_types.h"

#include <cstddef>

#include <glad/glad.h>
#include <memory>
#include <vector>

namespace lunalite::rhi {

class OpenGLCommandList;

struct OpenGLBuffer {
    GLuint id{0};
    BufferType type{BufferType::VertexBuffer};
    size_t size{0};
};

struct OpenGLShader {
    GLuint id{0};
    ShaderStage stage{ShaderStage::Vertex};
};

struct OpenGLPipeline {
    GLuint program{0};
    GLuint vao{0};
    GLenum topology{GL_TRIANGLES};
    VertexLayoutDesc vertex_layout;
    PipelineLayoutHandle layout{0};
    RenderTargetState render_target_state{};
    DepthState depth_state{};
    RasterState raster_state{};
};

struct OpenGLTexture {
    GLuint id{0};
    TextureDesc desc{};
    bool is_swapchain_backbuffer{false};
};

struct OpenGLTextureView {
    TextureHandle texture{0};
    TextureFormat format{TextureFormat::RGBA8};
    TextureAspect aspect{TextureAspect::Color};
    uint32_t base_mip_level{0};
    uint32_t mip_level_count{1};
    uint32_t base_array_layer{0};
    uint32_t array_layer_count{1};
};

struct OpenGLSampler {
    GLuint id{0};
    SamplerDesc desc{};
};

struct OpenGLBindGroupLayout {
    BindGroupLayoutDesc desc{};
    bool valid{true};
};

struct OpenGLBindGroup {
    BindGroupLayoutHandle layout{0};
    std::vector<BindGroupEntry> entries;
};

struct OpenGLPipelineLayout {
    PipelineLayoutDesc desc{};
    bool valid{true};
};

struct OpenGLFramebuffer {
    GLuint id{0};
    std::vector<TextureViewHandle> color_views;
    bool has_depth_stencil{false};
    TextureViewHandle depth_stencil_view{0};
    uint32_t width{0};
    uint32_t height{0};
};

class OpenGLDevice final : public Device {
public:
    OpenGLDevice();
    ~OpenGLDevice() override;

    BufferHandle createBuffer(const BufferDesc& desc, const void* data) override;
    void updateBuffer(BufferHandle buffer, const void* data, size_t size) override;
    void destroyBuffer(BufferHandle buffer) override;

    TextureHandle createTexture(const TextureDesc& desc) override;
    void updateTexture(TextureHandle texture, const TextureUploadDesc& desc) override;
    void destroyTexture(TextureHandle texture) override;

    TextureViewHandle createTextureView(const TextureViewDesc& desc) override;
    void destroyTextureView(TextureViewHandle view) override;

    SamplerHandle createSampler(const SamplerDesc& desc) override;
    void destroySampler(SamplerHandle sampler) override;

    BindGroupLayoutHandle createBindGroupLayout(const BindGroupLayoutDesc& desc) override;
    void destroyBindGroupLayout(BindGroupLayoutHandle layout) override;

    BindGroupHandle createBindGroup(const BindGroupDesc& desc) override;
    void updateBindGroup(BindGroupHandle group, const BindGroupDesc& desc) override;
    void destroyBindGroup(BindGroupHandle group) override;

    PipelineLayoutHandle createPipelineLayout(const PipelineLayoutDesc& desc) override;
    void destroyPipelineLayout(PipelineLayoutHandle layout) override;

    ShaderHandle createShader(const ShaderDesc& desc) override;
    void destroyShader(ShaderHandle shader) override;

    PipelineHandle createPipeline(const PipelineDesc& desc) override;
    void destroyPipeline(PipelineHandle pipeline) override;

    CommandList& getCommandList() override;

    OpenGLBuffer* getBuffer(BufferHandle handle);
    OpenGLTexture* getTexture(TextureHandle handle);
    OpenGLTextureView* getTextureView(TextureViewHandle handle);
    OpenGLSampler* getSampler(SamplerHandle handle);
    OpenGLBindGroupLayout* getBindGroupLayout(BindGroupLayoutHandle handle);
    OpenGLBindGroup* getBindGroup(BindGroupHandle handle);
    OpenGLPipelineLayout* getPipelineLayout(PipelineLayoutHandle handle);
    OpenGLShader* getShader(ShaderHandle handle);
    OpenGLPipeline* getPipeline(PipelineHandle handle);

    GLuint getFramebuffer(const RenderPassBeginInfo& info);
    TextureViewHandle createSwapchainTextureView(TextureFormat format);

private:
    std::vector<OpenGLBuffer> m_buffers;
    std::vector<OpenGLTexture> m_textures;
    std::vector<OpenGLTextureView> m_texture_views;
    std::vector<OpenGLSampler> m_samplers;
    std::vector<OpenGLBindGroupLayout> m_bind_group_layouts;
    std::vector<OpenGLBindGroup> m_bind_groups;
    std::vector<OpenGLPipelineLayout> m_pipeline_layouts;
    std::vector<OpenGLShader> m_shaders;
    std::vector<OpenGLPipeline> m_pipelines;
    std::vector<OpenGLFramebuffer> m_framebuffers;
    std::unique_ptr<OpenGLCommandList> m_command_list;
};

} // namespace lunalite::rhi
