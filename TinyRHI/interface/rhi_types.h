#pragma once

#include <cstddef>
#include <cstdint>

namespace lunalite::rhi {

template <typename Tag>
struct Handle {
    uint32_t value{0};

    constexpr Handle() = default;
    constexpr explicit Handle(uint32_t handle_value)
        : value(handle_value)
    {}

    constexpr explicit operator bool() const { return value != 0; }
};

template <typename Tag>
constexpr bool operator==(Handle<Tag> lhs, Handle<Tag> rhs)
{
    return lhs.value == rhs.value;
}

template <typename Tag>
constexpr bool operator!=(Handle<Tag> lhs, Handle<Tag> rhs)
{
    return !(lhs == rhs);
}

template <typename Tag>
constexpr uint32_t handleValue(Handle<Tag> handle)
{
    return handle.value;
}

template <typename Tag>
constexpr size_t handleIndex(Handle<Tag> handle)
{
    return static_cast<size_t>(handle.value - 1);
}

template <typename HandleType>
constexpr HandleType makeHandle(size_t index)
{
    return HandleType{static_cast<uint32_t>(index + 1)};
}

struct BufferHandleTag;
struct ShaderHandleTag;
struct PipelineHandleTag;
struct PipelineLayoutHandleTag;
struct TextureHandleTag;
struct TextureViewHandleTag;
struct SamplerHandleTag;
struct BindGroupLayoutHandleTag;
struct BindGroupHandleTag;
struct CommandListHandleTag;
struct SwapchainHandleTag;
struct SurfaceHandleTag;

using BufferHandle = Handle<BufferHandleTag>;
using ShaderHandle = Handle<ShaderHandleTag>;
using PipelineHandle = Handle<PipelineHandleTag>;
using PipelineLayoutHandle = Handle<PipelineLayoutHandleTag>;
using TextureHandle = Handle<TextureHandleTag>;
using TextureViewHandle = Handle<TextureViewHandleTag>;
using SamplerHandle = Handle<SamplerHandleTag>;
using BindGroupLayoutHandle = Handle<BindGroupLayoutHandleTag>;
using BindGroupHandle = Handle<BindGroupHandleTag>;
using CommandListHandle = Handle<CommandListHandleTag>;
using SwapchainHandle = Handle<SwapchainHandleTag>;
using SurfaceHandle = Handle<SurfaceHandleTag>;

enum class BackendType {
    OpenGL,
    Vulkan,
    D3D12,
    Metal
};

struct NativeWindowHandle;
using NativeSurfaceHandle = NativeWindowHandle;

} // namespace lunalite::rhi
