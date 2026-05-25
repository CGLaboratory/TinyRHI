#include "test_framework.h"

#include "TinyRHI/opengl/device.h"

using namespace lunalite::rhi;

TINYRHI_TEST_CASE("pipeline layout rejects zero sized push constant ranges")
{
    OpenGLDevice device;

    PipelineLayoutDesc desc{};
    desc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Vertex),
        .offset = 0,
        .size = 0,
    });

    TINYRHI_CHECK(device.createPipelineLayout(desc) == 0);
}

TINYRHI_TEST_CASE("pipeline layout rejects push constant ranges without stages")
{
    OpenGLDevice device;

    PipelineLayoutDesc desc{};
    desc.push_constants.push_back(PushConstantRange{
        .stages = 0,
        .offset = 0,
        .size = 16,
    });

    TINYRHI_CHECK(device.createPipelineLayout(desc) == 0);
}

TINYRHI_TEST_CASE("pipeline layout rejects overlapping push constant ranges in the same stage")
{
    OpenGLDevice device;

    PipelineLayoutDesc desc{};
    desc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Vertex),
        .offset = 0,
        .size = 16,
    });
    desc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Vertex),
        .offset = 8,
        .size = 16,
    });

    TINYRHI_CHECK(device.createPipelineLayout(desc) == 0);
}

TINYRHI_TEST_CASE("pipeline layout rejects overflowing push constant ranges")
{
    OpenGLDevice device;

    PipelineLayoutDesc desc{};
    desc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .offset = 0xFFFFFFF0u,
        .size = 32,
    });

    TINYRHI_CHECK(device.createPipelineLayout(desc) == 0);
}

TINYRHI_TEST_CASE("pipeline layout allows adjacent push constant ranges in the same stage")
{
    OpenGLDevice device;

    PipelineLayoutDesc desc{};
    desc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .offset = 0,
        .size = 16,
    });
    desc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .offset = 16,
        .size = 16,
    });

    TINYRHI_CHECK(device.createPipelineLayout(desc) != 0);
}

TINYRHI_TEST_CASE("pipeline layout allows overlapping push constant ranges in different stages")
{
    OpenGLDevice device;

    PipelineLayoutDesc desc{};
    desc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Vertex),
        .offset = 0,
        .size = 16,
    });
    desc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .offset = 0,
        .size = 16,
    });

    TINYRHI_CHECK(device.createPipelineLayout(desc) != 0);
}
