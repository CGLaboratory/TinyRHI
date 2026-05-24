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
    void beginRenderPass(const RenderPassBeginInfo& info) override;
    void endRenderPass() override;
    void setPipeline(PipelineHandle pipeline) override;
    void setBindGroup(uint32_t set, BindGroupHandle group) override;
    void setVertexBuffer(uint32_t slot, BufferHandle buffer) override;
    void setIndexBuffer(BufferHandle buffer, IndexFormat format) override;
    void setUniformBuffer(uint32_t binding, BufferHandle buffer) override;
    void resourceBarrier(const TextureBarrier* barriers, uint32_t count) override;
    void draw(uint32_t vertex_count, uint32_t first_vertex = 0) override;
    void drawIndexed(uint32_t index_count, uint32_t first_index = 0, int32_t vertex_offset = 0) override;

private:
    OpenGLDevice& m_device;
    PipelineHandle m_current_pipeline{0};
    BufferHandle m_current_index_buffer{0};
    IndexFormat m_current_index_format{IndexFormat::UInt32};
};

} // namespace lunalite::rhi
