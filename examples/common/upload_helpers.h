#pragma once

#include "TinyRHI/interface/device.h"

#include <cstddef>
#include <cstdint>

namespace tinyrhi_examples {

struct TextureUploadData {
    const void* data{nullptr};
    size_t size{0};
    size_t row_pitch{0};
    uint32_t x{0};
    uint32_t y{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t mip_level{0};
    uint32_t array_layer{0};
};

lunalite::rhi::BufferHandle createStaticBuffer(lunalite::rhi::Device& device,
                                               lunalite::rhi::CommandListHandle command_list,
                                               lunalite::rhi::BufferDesc desc,
                                               const void* data);

bool uploadTextureData(lunalite::rhi::Device& device,
                       lunalite::rhi::CommandListHandle command_list,
                       lunalite::rhi::TextureHandle texture,
                       const TextureUploadData& upload);

bool transitionTextureToShaderRead(lunalite::rhi::Device& device,
                                   lunalite::rhi::CommandListHandle command_list,
                                   lunalite::rhi::TextureHandle texture,
                                   bool generate_mipmaps);

} // namespace tinyrhi_examples
