#pragma once
#include "texture.h"

#include <cstdint>
#include <vector>

namespace lunalite::rhi {

enum class LoadOp {
    Load,
    Clear,
    DontCare
};

enum class StoreOp {
    Store,
    DontCare
};

struct ClearColor {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};
};

struct ColorAttachmentDesc {
    TextureViewHandle view{0};
    LoadOp load_op{LoadOp::Clear};
    StoreOp store_op{StoreOp::Store};
    ClearColor clear_color{};
};

struct DepthStencilAttachmentDesc {
    TextureViewHandle view{0};

    LoadOp depth_load_op{LoadOp::Clear};
    StoreOp depth_store_op{StoreOp::DontCare};
    float clear_depth{1.0f};

    // LoadOp stencil_load_op{LoadOp::DontCare};
    // StoreOp stencil_store_op{StoreOp::DontCare};
    // uint8_t clear_stencil{0};
};

struct RenderPassBeginInfo {
    std::vector<ColorAttachmentDesc> color_attachments;
    bool has_depth_stencil_attachment{false};
    DepthStencilAttachmentDesc depth_stencil_attachment{};
    uint32_t width{0};
    uint32_t height{0};
};

} // namespace lunalite::rhi
