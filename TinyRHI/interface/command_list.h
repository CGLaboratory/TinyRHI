#pragma once
#include "bind_group.h"
#include "buffer.h"
#include "pipeline.h"
#include "render_pass.h"
#include "rhi_types.h"
#include "texture.h"

namespace lunalite::rhi {

enum class ResourceState {
    Undefined,
    RenderTarget,
    DepthStencilWrite,
    ShaderRead,
    CopySrc,
    CopyDst,
    Present
};

struct TextureBarrier {
    TextureHandle texture{0};
    ResourceState old_state{ResourceState::Undefined};
    ResourceState new_state{ResourceState::Undefined};
};

class CommandList {
public:
    virtual ~CommandList() = default;

    virtual void begin() = 0;
    virtual void end() = 0;

    virtual void beginRenderPass(const RenderPassBeginInfo& info) = 0;
    virtual void endRenderPass() = 0;

    virtual void setPipeline(PipelineHandle pipeline) = 0;
    virtual void setBindGroup(uint32_t set, BindGroupHandle group) = 0;
    virtual void setVertexBuffer(uint32_t slot, BufferHandle buffer) = 0;
    virtual void setIndexBuffer(BufferHandle buffer, IndexFormat format) = 0;
    virtual void setUniformBuffer(uint32_t binding, BufferHandle buffer) = 0;
    virtual void resourceBarrier(const TextureBarrier* barriers, uint32_t count) = 0;
    virtual void draw(uint32_t vertex_count, uint32_t first_vertex = 0) = 0;
    virtual void drawIndexed(uint32_t index_count, uint32_t first_index = 0, int32_t vertex_offset = 0) = 0;
};
} // namespace lunalite::rhi
