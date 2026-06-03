#pragma once

namespace lunalite::rhi {

enum class ResourceState {
    Undefined,
    CopySrc,
    CopyDst,
    VertexBuffer,
    IndexBuffer,
    IndirectArgument,
    UniformRead,
    ShaderRead,
    StorageRead,
    StorageReadWrite,
    ColorAttachment,
    DepthStencilRead,
    DepthStencilWrite,
    Present
};

} // namespace lunalite::rhi
