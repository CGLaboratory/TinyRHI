#include "TinyRHI/backend_factory.h"
#include "common/win32_surface.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <thread>

using namespace lunalite::rhi;

namespace {

struct Vertex {
    float position[3];
};

struct UniformData {
    float color[4];
};

constexpr size_t kUniformStride = 256;

constexpr const char* kVertexShader = R"GLSL(
#version 450 core
layout(location = 0) in vec3 inPosition;

void main()
{
    gl_Position = vec4(inPosition, 1.0);
}
)GLSL";

constexpr const char* kFragmentShader = R"GLSL(
#version 450 core
layout(std140, binding = 0) uniform Params {
    vec4 uColor;
};

out vec4 outColor;

void main()
{
    outColor = uColor;
}
)GLSL";

} // namespace

int main()
{
    auto instance = BackendFactory::createInstance(BackendType::OpenGL);
    if (!instance) {
        std::printf("Failed to create OpenGL backend.\n");
        return 1;
    }

    tinyrhi_examples::Win32Surface surface;
    if (!surface.create("TinyRHI Dynamic Uniform Offsets", 960, 540)) {
        std::printf("Failed to create Win32 surface.\n");
        return 1;
    }

    if (!instance->init()) {
        std::printf("Failed to initialize TinyRHI instance.\n");
        return 1;
    }

    auto* device = instance->getDevice();
    const SwapchainHandle swapchainHandle = device->createSwapchain(surface, SwapchainDesc{});
    auto* swapchain = device->getSwapchain(swapchainHandle);
    if (swapchain == nullptr) {
        std::printf("Failed to create swapchain.\n");
        instance->shutdown();
        return 1;
    }

    auto& commands = device->getCommandList();

    constexpr std::array<Vertex, 6> vertices = {{
        {{-0.82f, 0.58f, 0.0f}},
        {{-0.94f, -0.48f, 0.0f}},
        {{-0.22f, -0.48f, 0.0f}},
        {{0.22f, 0.58f, 0.0f}},
        {{0.94f, -0.48f, 0.0f}},
        {{0.82f, 0.58f, 0.0f}},
    }};

    std::array<unsigned char, kUniformStride * 2> uniformData{};
    auto* leftColor = reinterpret_cast<UniformData*>(uniformData.data());
    auto* rightColor = reinterpret_cast<UniformData*>(uniformData.data() + kUniformStride);
    *leftColor = UniformData{{0.92f, 0.24f, 0.18f, 1.0f}};
    *rightColor = UniformData{{0.18f, 0.45f, 0.95f, 1.0f}};

    BufferHandle vertexBuffer = device->createBuffer(
        BufferDesc{.size = sizeof(vertices), .usage = BufferUsage::Vertex | BufferUsage::CopyDst},
        vertices.data());
    BufferHandle uniformBuffer = device->createBuffer(
        BufferDesc{.size = uniformData.size(), .usage = BufferUsage::Uniform | BufferUsage::CopyDst, .memory = MemoryUsage::CpuToGpu},
        uniformData.data());
    ShaderHandle vertexShader = device->createShader(ShaderDesc{.stage = ShaderStage::Vertex, .source = kVertexShader});
    ShaderHandle fragmentShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Fragment, .source = kFragmentShader});

    BindGroupLayoutDesc bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::UniformBuffer,
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .count = 1,
        .dynamic_offset = true,
    });
    BindGroupLayoutHandle bindGroupLayout = device->createBindGroupLayout(bindGroupLayoutDesc);

    PipelineLayoutDesc pipelineLayoutDesc{};
    pipelineLayoutDesc.bind_group_layouts.push_back(bindGroupLayout);
    PipelineLayoutHandle layout = device->createPipelineLayout(pipelineLayoutDesc);

    BindGroupDesc bindGroupDesc{};
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::UniformBuffer,
        .buffer = BufferBinding{.buffer = uniformBuffer, .offset = 0, .size = sizeof(UniformData)},
    });
    BindGroupHandle bindGroup = device->createBindGroup(bindGroupDesc);

    PipelineDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Triangle;
    pipelineDesc.vertex_input = VertexInputDesc{
        .buffers =
            {
                VertexBufferLayoutDesc{
                    .binding = 0,
                    .stride = sizeof(Vertex),
                    .attributes =
                        {
                            VertexAttributeDesc{.location = 0, .format = VertexFormat::Float3, .offset = 0},
                        },
                },
            },
    };
    pipelineDesc.layout = layout;
    pipelineDesc.vertex_shader = vertexShader;
    pipelineDesc.fragment_shader = fragmentShader;
    pipelineDesc.render_target_state.color_targets.push_back(ColorTargetState{.format = TextureFormat::RGBA8});
    pipelineDesc.depth_state.enabled = false;

    PipelineHandle pipeline = device->createPipeline(pipelineDesc);
    if (vertexBuffer == 0 || uniformBuffer == 0 || vertexShader == 0 || fragmentShader == 0 || bindGroupLayout == 0 ||
        layout == 0 || bindGroup == 0 || pipeline == 0) {
        std::printf("Failed to create dynamic uniform offset resources.\n");
        instance->shutdown();
        return 1;
    }

    while (surface.pollEvents() && !surface.shouldClose()) {
        swapchain->resize(surface.getWidth(), surface.getHeight());

        RenderPassBeginInfo pass{};
        pass.color_attachments.push_back(ColorAttachmentDesc{
            .view = swapchain->getCurrentColorTextureView(),
            .load_op = LoadOp::Clear,
            .store_op = StoreOp::Store,
            .clear_color = ClearColor{0.05f, 0.055f, 0.065f, 1.0f},
        });
        pass.width = swapchain->getWidth();
        pass.height = swapchain->getHeight();

        commands.begin();
        commands.beginRenderPass(pass);
        commands.setPipeline(pipeline);
        commands.setVertexBuffer(0, vertexBuffer);

        uint32_t offset = 0;
        commands.setBindGroup(0, bindGroup, &offset, 1);
        commands.draw(3, 0);

        offset = static_cast<uint32_t>(kUniformStride);
        commands.setBindGroup(0, bindGroup, &offset, 1);
        commands.draw(3, 3);

        commands.endRenderPass();
        commands.end();

        swapchain->present();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    device->destroyPipeline(pipeline);
    device->destroyBindGroup(bindGroup);
    device->destroyPipelineLayout(layout);
    device->destroyBindGroupLayout(bindGroupLayout);
    device->destroyShader(fragmentShader);
    device->destroyShader(vertexShader);
    device->destroyBuffer(uniformBuffer);
    device->destroyBuffer(vertexBuffer);
    instance->shutdown();
    return 0;
}
