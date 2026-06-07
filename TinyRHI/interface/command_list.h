#pragma once
#include "bind_group.h"
#include "buffer.h"
#include "pipeline.h"
#include "render_pass.h"
#include "resource_state.h"
#include "rhi_types.h"
#include "texture.h"
#include "timestamp_query.h"

#include <cstddef>

namespace lunalite::rhi {

struct BufferTransition {
    BufferHandle buffer{};
    ResourceState state{ResourceState::Undefined};
};

struct TextureTransition {
    TextureHandle texture{};
    ResourceState state{ResourceState::Undefined};
    TextureSubresourceRange range{};
};

struct BufferCopyRegion {
    size_t src_offset{0};
    size_t dst_offset{0};
    size_t size{0};
};

struct BufferTextureCopyRegion {
    size_t buffer_offset{0};
    size_t buffer_row_pitch{0};
    uint32_t texture_x{0};
    uint32_t texture_y{0};
    uint32_t texture_width{0};
    uint32_t texture_height{0};
    uint32_t mip_level{0};
    uint32_t array_layer{0};
};

struct Viewport {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};
    float min_depth{0.0f};
    float max_depth{1.0f};
};

struct ScissorRect {
    int32_t x{0};
    int32_t y{0};
    uint32_t width{0};
    uint32_t height{0};
};

class CommandList {
public:
    virtual ~CommandList() = default;

    virtual void begin() = 0;
    virtual void end() = 0;

    virtual void resetTimestampQueries(TimestampQueryPoolHandle pool, uint32_t first, uint32_t count) = 0;
    virtual void writeTimestamp(TimestampQueryPoolHandle pool, uint32_t index) = 0;

    virtual void beginRenderPass(const RenderPassBeginInfo& info) = 0;
    virtual void endRenderPass() = 0;

    virtual void setPipeline(PipelineHandle pipeline) = 0;
    virtual void setBindGroup(uint32_t set,
                              BindGroupHandle group,
                              const uint32_t* dynamic_offsets = nullptr,
                              uint32_t dynamic_offset_count = 0) = 0;

    virtual void setVertexBuffer(uint32_t slot, BufferHandle buffer, size_t offset = 0) = 0;
    virtual void setIndexBuffer(BufferHandle buffer, IndexFormat format, size_t offset = 0) = 0;

    virtual void setViewport(uint32_t first, const Viewport* viewports, uint32_t count) = 0;
    virtual void setScissor(uint32_t first, const ScissorRect* scissors, uint32_t count) = 0;

    virtual void pushConstants(ShaderStageFlags stages, uint32_t offset, uint32_t size, const void* data) = 0;

    virtual void transition(const BufferTransition* transitions, uint32_t count) = 0;
    virtual void transition(const TextureTransition* transitions, uint32_t count) = 0;
    virtual void
        copyBufferToBuffer(BufferHandle src, BufferHandle dst, const BufferCopyRegion* regions, uint32_t count) = 0;
    virtual void copyBufferToTexture(BufferHandle src,
                                     TextureHandle dst,
                                     const BufferTextureCopyRegion* regions,
                                     uint32_t count) = 0;
    virtual void generateMipmaps(TextureHandle texture) = 0;

    virtual void draw(uint32_t vertex_count, uint32_t first_vertex = 0) = 0;
    virtual void drawIndexed(uint32_t index_count, uint32_t first_index = 0, int32_t vertex_offset = 0) = 0;
    virtual void dispatch(uint32_t group_count_x, uint32_t group_count_y = 1, uint32_t group_count_z = 1) = 0;
};
} // namespace lunalite::rhi
