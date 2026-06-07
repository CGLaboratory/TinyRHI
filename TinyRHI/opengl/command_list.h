#pragma once
#include "../interface/command_list.h"

#include <glad/glad.h>

namespace lunalite::rhi {

class OpenGLDevice;

class OpenGLCommandList final : public CommandList {
public:
    explicit OpenGLCommandList(OpenGLDevice& device);
    ~OpenGLCommandList() override = default;

    void begin() override;
    void end() override;
    void resetTimestampQueries(TimestampQueryPoolHandle pool, uint32_t first, uint32_t count) override;
    void writeTimestamp(TimestampQueryPoolHandle pool, uint32_t index) override;
    void beginRenderPass(const RenderPassBeginInfo& info) override;
    void endRenderPass() override;
    void setPipeline(PipelineHandle pipeline) override;
    void setBindGroup(uint32_t set,
                      BindGroupHandle group,
                      const uint32_t* dynamic_offsets = nullptr,
                      uint32_t dynamic_offset_count = 0) override;
    void setVertexBuffer(uint32_t slot, BufferHandle buffer, size_t offset = 0) override;
    void setIndexBuffer(BufferHandle buffer, IndexFormat format, size_t offset = 0) override;
    void setViewport(uint32_t first, const Viewport* viewports, uint32_t count) override;
    void setScissor(uint32_t first, const ScissorRect* scissors, uint32_t count) override;
    void pushConstants(ShaderStageFlags stages, uint32_t offset, uint32_t size, const void* data) override;
    void transition(const BufferTransition* transitions, uint32_t count) override;
    void transition(const TextureTransition* transitions, uint32_t count) override;
    void copyBufferToBuffer(BufferHandle src,
                            BufferHandle dst,
                            const BufferCopyRegion* regions,
                            uint32_t count) override;
    void copyBufferToTexture(BufferHandle src,
                             TextureHandle dst,
                             const BufferTextureCopyRegion* regions,
                             uint32_t count) override;
    void generateMipmaps(TextureHandle texture) override;
    void draw(uint32_t vertex_count, uint32_t first_vertex = 0) override;
    void drawIndexed(uint32_t index_count, uint32_t first_index = 0, int32_t vertex_offset = 0) override;
    void dispatch(uint32_t group_count_x, uint32_t group_count_y = 1, uint32_t group_count_z = 1) override;

private:
    OpenGLDevice& m_device;
    PipelineHandle m_current_pipeline{};
    BufferHandle m_current_index_buffer{};
    IndexFormat m_current_index_format{IndexFormat::UInt32};
    size_t m_current_index_buffer_offset{0};
};

} // namespace lunalite::rhi
