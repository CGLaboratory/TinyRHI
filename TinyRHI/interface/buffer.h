#pragma once
#include "rhi_types.h"

#include <cstddef>
#include <cstdint>

namespace lunalite::rhi {

enum class BufferUsage : uint32_t {
    None = 0,
    Vertex = 1 << 0,
    Index = 1 << 1,
    Uniform = 1 << 2,
    Storage = 1 << 3,
    CopySrc = 1 << 4,
    CopyDst = 1 << 5,
    Indirect = 1 << 6
};

constexpr BufferUsage operator|(BufferUsage lhs, BufferUsage rhs)
{
    return static_cast<BufferUsage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr BufferUsage operator&(BufferUsage lhs, BufferUsage rhs)
{
    return static_cast<BufferUsage>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr BufferUsage& operator|=(BufferUsage& lhs, BufferUsage rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

enum class MemoryUsage {
    GpuOnly,
    CpuToGpu,
    GpuToCpu
};

enum class IndexFormat {
    UInt16,
    UInt32
};

struct BufferDesc {
    size_t size{0};
    BufferUsage usage{BufferUsage::None};
    MemoryUsage memory{MemoryUsage::GpuOnly};
};

} // namespace lunalite::rhi
