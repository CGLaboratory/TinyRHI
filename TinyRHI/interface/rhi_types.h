#pragma once

#include <cstdint>

namespace lunalite::rhi {

using BufferHandle = uint32_t;
using ShaderHandle = uint32_t;
using PipelineHandle = uint32_t;
using PipelineLayoutHandle = uint32_t;
using TextureHandle = uint32_t;
using TextureViewHandle = uint32_t;
using SamplerHandle = uint32_t;
using BindGroupLayoutHandle = uint32_t;
using BindGroupHandle = uint32_t;
using SwapchainHandle = uint32_t;

enum class BackendType {
    OpenGL,
    Vulkan,
    D3D12,
    Metal
};

} // namespace lunalite::rhi
