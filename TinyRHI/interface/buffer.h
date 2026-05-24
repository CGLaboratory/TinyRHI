#pragma once
#include "rhi_types.h"

#include <cstddef>

namespace lunalite::rhi {

enum class BufferType {
    VertexBuffer,
    IndexBuffer,
    UniformBuffer,
    StorageBuffer
};

enum class BufferUsage {
    Static,
    Dynamic
};

enum class IndexFormat {
    UInt16,
    UInt32
};

struct BufferDesc {
    BufferType type;
    BufferUsage usage;
    size_t size;
};

} // namespace lunalite::rhi
