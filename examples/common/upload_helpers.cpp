#include "common/upload_helpers.h"

namespace tinyrhi_examples {

using namespace lunalite::rhi;

BufferHandle createStaticBuffer(Device& device, CommandListHandle command_list, BufferDesc desc, const void* data)
{
    if (data == nullptr || desc.size == 0) {
        return {};
    }

    const ResourceState finalState = desc.initial_state;
    desc.usage |= BufferUsage::CopyDst;
    desc.initial_state = ResourceState::CopyDst;

    BufferHandle target = device.createBuffer(desc);
    BufferHandle staging = device.createBuffer(BufferDesc{
        .size = desc.size,
        .usage = BufferUsage::CopySrc,
        .memory = MemoryUsage::CpuToGpu,
        .initial_state = ResourceState::CopySrc,
    });
    auto* commands = device.getCommandList(command_list);
    if (!target || !staging || commands == nullptr) {
        if (staging) {
            device.destroyBuffer(staging);
        }
        if (target) {
            device.destroyBuffer(target);
        }
        return {};
    }

    device.updateBuffer(staging, 0, data, desc.size);

    BufferCopyRegion copy{
        .src_offset = 0,
        .dst_offset = 0,
        .size = desc.size,
    };
    commands->begin();
    commands->copyBufferToBuffer(staging, target, &copy, 1);
    if (finalState != ResourceState::Undefined && finalState != ResourceState::CopyDst) {
        BufferTransition ready{
            .buffer = target,
            .state = finalState,
        };
        commands->transition(&ready, 1);
    }
    commands->end();
    device.submit(command_list);
    device.destroyBuffer(staging);
    return target;
}

bool uploadTextureData(Device& device,
                       CommandListHandle command_list,
                       TextureHandle texture,
                       const TextureUploadData& upload)
{
    if (!texture || upload.data == nullptr || upload.size == 0 || upload.width == 0 || upload.height == 0) {
        return false;
    }

    BufferHandle staging = device.createBuffer(BufferDesc{
        .size = upload.size,
        .usage = BufferUsage::CopySrc,
        .memory = MemoryUsage::CpuToGpu,
        .initial_state = ResourceState::CopySrc,
    });
    auto* commands = device.getCommandList(command_list);
    if (!staging || commands == nullptr) {
        if (staging) {
            device.destroyBuffer(staging);
        }
        return false;
    }

    device.updateBuffer(staging, 0, upload.data, upload.size);

    BufferTextureCopyRegion copy{
        .buffer_offset = 0,
        .buffer_row_pitch = upload.row_pitch,
        .texture_x = upload.x,
        .texture_y = upload.y,
        .texture_width = upload.width,
        .texture_height = upload.height,
        .mip_level = upload.mip_level,
        .array_layer = upload.array_layer,
    };
    commands->begin();
    commands->copyBufferToTexture(staging, texture, &copy, 1);
    commands->end();
    device.submit(command_list);
    device.destroyBuffer(staging);
    return true;
}

bool transitionTextureToShaderRead(Device& device,
                                   CommandListHandle command_list,
                                   TextureHandle texture,
                                   bool generate_mipmaps)
{
    auto* commands = device.getCommandList(command_list);
    if (!texture || commands == nullptr) {
        return false;
    }

    TextureTransition ready{
        .texture = texture,
        .state = ResourceState::ShaderRead,
    };
    commands->begin();
    if (generate_mipmaps) {
        commands->generateMipmaps(texture);
    }
    commands->transition(&ready, 1);
    commands->end();
    device.submit(command_list);
    return true;
}

} // namespace tinyrhi_examples
