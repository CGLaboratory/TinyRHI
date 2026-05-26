#pragma once

#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/instance.h"

#include <imgui.h>

#include <cstddef>
#include <vector>

namespace tinyrhi_examples {

class TinyRHIImGuiRenderer final {
public:
    TinyRHIImGuiRenderer() = default;
    ~TinyRHIImGuiRenderer();

    TinyRHIImGuiRenderer(const TinyRHIImGuiRenderer&) = delete;
    TinyRHIImGuiRenderer& operator=(const TinyRHIImGuiRenderer&) = delete;

    bool init(lunalite::rhi::Device& device);
    void setSurfaceOwner(lunalite::rhi::Instance& instance);
    void shutdown();
    void render(ImDrawData* draw_data, lunalite::rhi::CommandList& commands);

private:
    bool createStaticResources();
    bool createFontTexture();
    bool ensureBuffers(int vertex_count, int index_count);
    void initViewportSupport();
    void shutdownViewportSupport();
    void createViewportWindow(ImGuiViewport* viewport);
    void destroyViewportWindow(ImGuiViewport* viewport);
    void setViewportWindowSize(ImGuiViewport* viewport, ImVec2 size);
    void renderViewportWindow(ImGuiViewport* viewport);
    void swapViewportBuffers(ImGuiViewport* viewport);
    void destroyBuffers();
    void destroyFontTexture();
    lunalite::rhi::BindGroupHandle bindGroupFromTextureId(ImTextureID texture_id) const;

    static TinyRHIImGuiRenderer* currentRenderer();
    static void createViewportWindowCallback(ImGuiViewport* viewport);
    static void destroyViewportWindowCallback(ImGuiViewport* viewport);
    static void setViewportWindowSizeCallback(ImGuiViewport* viewport, ImVec2 size);
    static void renderViewportWindowCallback(ImGuiViewport* viewport, void* render_arg);
    static void swapViewportBuffersCallback(ImGuiViewport* viewport, void* render_arg);

    lunalite::rhi::Device* m_device{nullptr};
    lunalite::rhi::Instance* m_instance{nullptr};
    lunalite::rhi::BufferHandle m_vertex_buffer{0};
    lunalite::rhi::BufferHandle m_index_buffer{0};
    size_t m_vertex_buffer_size{0};
    size_t m_index_buffer_size{0};
    lunalite::rhi::ShaderHandle m_vertex_shader{0};
    lunalite::rhi::ShaderHandle m_fragment_shader{0};
    lunalite::rhi::SamplerHandle m_sampler{0};
    lunalite::rhi::BindGroupLayoutHandle m_bind_group_layout{0};
    lunalite::rhi::PipelineLayoutHandle m_pipeline_layout{0};
    lunalite::rhi::PipelineHandle m_pipeline{0};
    lunalite::rhi::TextureHandle m_font_texture{0};
    lunalite::rhi::TextureViewHandle m_font_texture_view{0};
    lunalite::rhi::BindGroupHandle m_font_bind_group{0};
    std::vector<ImDrawVert> m_vertex_upload;
    std::vector<ImDrawIdx> m_index_upload;
};

} // namespace tinyrhi_examples
