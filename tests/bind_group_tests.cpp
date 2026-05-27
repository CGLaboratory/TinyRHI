#include "test_framework.h"

#include "TinyRHI/opengl/device.h"

using namespace lunalite::rhi;

namespace {

BindGroupLayoutDesc singleUniformLayoutDesc()
{
    BindGroupLayoutDesc desc{};
    desc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::UniformBuffer,
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .count = 1,
    });
    return desc;
}

} // namespace

TINYRHI_TEST_CASE("bind group layout rejects duplicate bindings")
{
    OpenGLDevice device;

    BindGroupLayoutDesc desc = singleUniformLayoutDesc();
    desc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::Sampler,
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .count = 1,
    });

    TINYRHI_CHECK(!device.createBindGroupLayout(desc));
}

TINYRHI_TEST_CASE("bind group layout rejects unsupported descriptor arrays")
{
    OpenGLDevice device;

    BindGroupLayoutDesc desc = singleUniformLayoutDesc();
    desc.entries[0].count = 2;

    TINYRHI_CHECK(!device.createBindGroupLayout(desc));
}

TINYRHI_TEST_CASE("bind group layout rejects dynamic offsets on non buffer bindings")
{
    OpenGLDevice device;

    BindGroupLayoutDesc desc{};
    desc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::Sampler,
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .count = 1,
        .dynamic_offset = true,
    });

    TINYRHI_CHECK(!device.createBindGroupLayout(desc));
}

TINYRHI_TEST_CASE("bind group rejects missing layout bindings")
{
    OpenGLDevice device;

    const BindGroupLayoutHandle layout = device.createBindGroupLayout(singleUniformLayoutDesc());
    TINYRHI_REQUIRE(!!layout);

    BindGroupDesc desc{};
    desc.layout = layout;

    TINYRHI_CHECK(!device.createBindGroup(desc));
}

TINYRHI_TEST_CASE("bind group rejects duplicate entries")
{
    OpenGLDevice device;

    BindGroupLayoutDesc layoutDesc{};
    layoutDesc.entries.push_back(BindGroupLayoutEntry{.binding = 0, .type = BindingType::UniformBuffer});
    layoutDesc.entries.push_back(BindGroupLayoutEntry{.binding = 1, .type = BindingType::UniformBuffer});
    const BindGroupLayoutHandle layout = device.createBindGroupLayout(layoutDesc);
    TINYRHI_REQUIRE(!!layout);

    BindGroupDesc desc{};
    desc.layout = layout;
    desc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::UniformBuffer,
    });
    desc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::UniformBuffer,
    });

    TINYRHI_CHECK(!device.createBindGroup(desc));
}

TINYRHI_TEST_CASE("bind group rejects entries outside the layout")
{
    OpenGLDevice device;

    const BindGroupLayoutHandle layout = device.createBindGroupLayout(singleUniformLayoutDesc());
    TINYRHI_REQUIRE(!!layout);

    BindGroupDesc desc{};
    desc.layout = layout;
    desc.entries.push_back(BindGroupEntry{
        .binding = 1,
        .type = BindingType::UniformBuffer,
    });

    TINYRHI_CHECK(!device.createBindGroup(desc));
}

TINYRHI_TEST_CASE("bind group rejects entry type mismatches")
{
    OpenGLDevice device;

    const BindGroupLayoutHandle layout = device.createBindGroupLayout(singleUniformLayoutDesc());
    TINYRHI_REQUIRE(!!layout);

    BindGroupDesc desc{};
    desc.layout = layout;
    desc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::Sampler,
    });

    TINYRHI_CHECK(!device.createBindGroup(desc));
}
