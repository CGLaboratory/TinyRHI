#pragma once
#include "rhi_types.h"

namespace lunalite::rhi {

enum class ShaderStage : uint32_t {
    Vertex = 1 << 0,
    Fragment = 1 << 1
    // Compute
};

using ShaderStageFlags = uint32_t;

constexpr ShaderStageFlags operator|(ShaderStage lhs, ShaderStage rhs)
{
    return static_cast<ShaderStageFlags>(lhs) | static_cast<ShaderStageFlags>(rhs);
}

constexpr ShaderStageFlags shaderStageFlag(ShaderStage stage)
{
    return static_cast<ShaderStageFlags>(stage);
}

struct ShaderDesc {
    ShaderStage stage;
    const char* source;
};

} // namespace lunalite::rhi
