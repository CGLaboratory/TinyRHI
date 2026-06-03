#include "common/win32_window.h"
#include "TinyRHI/backend_factory.h"

#include <cstddef>
#include <cstdio>

#include <chrono>
#include <thread>

using namespace lunalite::rhi;

namespace {

constexpr uint32_t kParticleCount = 1'024;
constexpr uint32_t kComputeGroupSize = 64;

struct Particle {
    float position[4];
    float color[4];
};

constexpr const char* kComputeShader = R"GLSL(
#version 450 core
layout(local_size_x = 64) in;

struct Particle {
    vec4 position;
    vec4 color;
};

layout(std430, binding = 0) buffer ParticleBuffer
{
    Particle particles[];
};

uniform vec4 uPushConstants[1];

void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index >= 1024u) {
        return;
    }

    float fi = float(index);
    float time = uPushConstants[0].x;
    float angle = fi * 0.0618 + time;
    float ring = 0.15 + 0.75 * fract(fi * 0.017);
    vec2 position = vec2(cos(angle), sin(angle)) * ring;

    particles[index].position = vec4(position, 0.0, 1.0);
    particles[index].color = vec4(
        0.55 + 0.45 * cos(angle),
        0.55 + 0.45 * sin(angle * 0.7),
        1.0 - ring * 0.45,
        1.0);
}
)GLSL";

constexpr const char* kVertexShader = R"GLSL(
#version 450 core
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inColor;

out vec4 vertexColor;

void main()
{
    gl_Position = inPosition;
    gl_PointSize = 4.0;
    vertexColor = inColor;
}
)GLSL";

constexpr const char* kFragmentShader = R"GLSL(
#version 450 core
in vec4 vertexColor;
out vec4 outColor;

void main()
{
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    float alpha = smoothstep(1.0, 0.25, dot(p, p));
    if (alpha <= 0.01) {
        discard;
    }

    outColor = vec4(vertexColor.rgb, alpha);
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

    tinyrhi_examples::Win32Window surface;
    if (!surface.create("TinyRHI Compute Particles", 960, 540)) {
        std::printf("Failed to create Win32 surface.\n");
        return 1;
    }

    if (!instance->init()) {
        std::printf("Failed to initialize TinyRHI instance.\n");
        return 1;
    }

    auto* device = instance->getDevice();
    const SurfaceHandle surfaceHandle = instance->createSurface(surface.nativeWindow());
    const SwapchainHandle swapchainHandle = device->createSwapchain(surfaceHandle, SwapchainDesc{});
    auto* swapchain = device->getSwapchain(swapchainHandle);
    if (swapchain == nullptr) {
        std::printf("Failed to create swapchain.\n");
        instance->shutdown();
        return 1;
    }

    const CommandListHandle commandListHandle = device->createCommandList();
    auto* commandList = device->getCommandList(commandListHandle);
    if (commandList == nullptr) {
        std::printf("Failed to create command list.\n");
        instance->shutdown();
        return 1;
    }
    auto& commands = *commandList;

    BufferHandle particleBuffer = device->createBuffer(BufferDesc{
        .size = sizeof(Particle) * kParticleCount,
        .usage = BufferUsage::Vertex | BufferUsage::Storage,
        .memory = MemoryUsage::GpuOnly,
        .initial_state = ResourceState::VertexBuffer,
    });

    ShaderHandle computeShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Compute, .source = kComputeShader});
    ShaderHandle vertexShader = device->createShader(ShaderDesc{.stage = ShaderStage::Vertex, .source = kVertexShader});
    ShaderHandle fragmentShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Fragment, .source = kFragmentShader});

    BindGroupLayoutDesc computeBindGroupLayoutDesc{};
    computeBindGroupLayoutDesc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::StorageBuffer,
        .stages = shaderStageFlag(ShaderStage::Compute),
        .count = 1,
    });
    BindGroupLayoutHandle computeBindGroupLayout = device->createBindGroupLayout(computeBindGroupLayoutDesc);

    PipelineLayoutDesc computeLayoutDesc{};
    computeLayoutDesc.bind_group_layouts.push_back(computeBindGroupLayout);
    computeLayoutDesc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Compute),
        .offset = 0,
        .size = 16,
    });
    PipelineLayoutHandle computeLayout = device->createPipelineLayout(computeLayoutDesc);

    BindGroupDesc particleBindGroupDesc{};
    particleBindGroupDesc.layout = computeBindGroupLayout;
    particleBindGroupDesc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::StorageBuffer,
        .buffer =
            BufferBinding{
                .buffer = particleBuffer,
                .offset = 0,
                .size = sizeof(Particle) * kParticleCount,
            },
    });
    BindGroupHandle particleBindGroup = device->createBindGroup(particleBindGroupDesc);

    PipelineHandle computePipeline = device->createComputePipeline(ComputePipelineDesc{
        .layout = computeLayout,
        .compute_shader = computeShader,
    });

    PipelineLayoutHandle graphicsLayout = device->createPipelineLayout(PipelineLayoutDesc{});

    PipelineDesc graphicsDesc{};
    graphicsDesc.topology = PrimitiveTopology::Point;
    graphicsDesc.vertex_input = VertexInputDesc{
        .buffers =
            {
                VertexBufferLayoutDesc{
                    .binding = 0,
                    .stride = sizeof(Particle),
                    .attributes =
                        {
                            VertexAttributeDesc{
                                .location = 0,
                                .format = VertexFormat::Float4,
                                .offset = offsetof(Particle, position),
                            },
                            VertexAttributeDesc{
                                .location = 1,
                                .format = VertexFormat::Float4,
                                .offset = offsetof(Particle, color),
                            },
                        },
                },
            },
    };
    graphicsDesc.layout = graphicsLayout;
    graphicsDesc.vertex_shader = vertexShader;
    graphicsDesc.fragment_shader = fragmentShader;
    ColorTargetState colorTarget{};
    colorTarget.blend.enabled = true;
    graphicsDesc.render_target_state.color_targets.push_back(colorTarget);
    graphicsDesc.depth_state.enabled = false;
    PipelineHandle graphicsPipeline = device->createPipeline(graphicsDesc);

    if (!particleBuffer || !computeShader || !vertexShader || !fragmentShader || !computeBindGroupLayout ||
        !computeLayout || !particleBindGroup || !computePipeline || !graphicsLayout || !graphicsPipeline) {
        std::printf("Failed to create compute particle resources.\n");
        instance->shutdown();
        return 1;
    }

    const auto startTime = std::chrono::steady_clock::now();
    while (surface.pollEvents() && !surface.shouldClose()) {
        swapchain->resize(surface.getWidth(), surface.getHeight());
        SwapchainFrame frame{};
        if (!device->beginFrame(swapchainHandle, frame)) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        const float elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - startTime).count();
        const float constants[4] = {elapsed, 0.0f, 0.0f, 0.0f};

        RenderPassBeginInfo pass{};
        pass.color_attachments.push_back(ColorAttachmentDesc{
            .view = frame.color_view,
            .load_op = LoadOp::Clear,
            .store_op = StoreOp::Store,
            .clear_color = ClearColor{0.03f, 0.035f, 0.045f, 1.0f},
        });
        pass.width = frame.width;
        pass.height = frame.height;

        commands.begin();
        BufferTransition particleStorageTransition{
            .buffer = particleBuffer,
            .state = ResourceState::StorageReadWrite,
        };
        commands.transition(&particleStorageTransition, 1);
        commands.setPipeline(computePipeline);
        commands.setBindGroup(0, particleBindGroup);
        commands.pushConstants(shaderStageFlag(ShaderStage::Compute), 0, sizeof(constants), constants);
        commands.dispatch((kParticleCount + kComputeGroupSize - 1) / kComputeGroupSize);

        BufferTransition particleVertexTransition{
            .buffer = particleBuffer,
            .state = ResourceState::VertexBuffer,
        };
        commands.transition(&particleVertexTransition, 1);

        commands.beginRenderPass(pass);
        commands.setPipeline(graphicsPipeline);
        commands.setVertexBuffer(0, particleBuffer);
        commands.draw(kParticleCount);
        commands.endRenderPass();
        commands.end();

        device->submit(commandListHandle, &frame);
        device->present(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    device->destroyPipeline(graphicsPipeline);
    device->destroyPipelineLayout(graphicsLayout);
    device->destroyPipeline(computePipeline);
    device->destroyBindGroup(particleBindGroup);
    device->destroyPipelineLayout(computeLayout);
    device->destroyBindGroupLayout(computeBindGroupLayout);
    device->destroyShader(fragmentShader);
    device->destroyShader(vertexShader);
    device->destroyShader(computeShader);
    device->destroyBuffer(particleBuffer);
    instance->shutdown();
    return 0;
}
