#pragma once
#include "buffer.h"
#include "rhi_types.h"
#include "shader.h"
#include "texture.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lunalite::rhi {

enum class BindingType {
    UniformBuffer,
    StorageBuffer,
    SampledTexture,
    Sampler,
    CombinedImageSampler
};

struct BindGroupLayoutEntry {
    uint32_t binding{0};
    BindingType type{BindingType::UniformBuffer};
    ShaderStageFlags stages{shaderStageFlag(ShaderStage::Vertex) | shaderStageFlag(ShaderStage::Fragment)};
    uint32_t count{1};
};

struct BindGroupLayoutDesc {
    std::vector<BindGroupLayoutEntry> entries;
};

struct BufferBinding {
    BufferHandle buffer{0};
    size_t offset{0};
    size_t size{0};
};

struct BindGroupEntry {
    uint32_t binding{0};
    BindingType type{BindingType::UniformBuffer};
    BufferBinding buffer{};
    TextureViewHandle texture_view{0};
    SamplerHandle sampler{0};
};

struct BindGroupDesc {
    BindGroupLayoutHandle layout{0};
    std::vector<BindGroupEntry> entries;
};

} // namespace lunalite::rhi
