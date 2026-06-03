#include "common/win32_window.h"
#include "TinyRHI/backend_factory.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <array>
#include <thread>

using namespace lunalite::rhi;

namespace {

struct Vertex {
    float position[3];
    float color[4];
};

constexpr const char* kVertexShader = R"GLSL(
#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 3) in vec4 inColor;

out vec4 vColor;

void main()
{
    gl_Position = vec4(inPosition, 1.0);
    vColor = inColor;
}
)GLSL";

constexpr const char* kFragmentShader = R"GLSL(
#version 450 core
in vec4 vColor;
out vec4 outColor;

void main()
{
    outColor = vColor;
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
    if (!surface.create("TinyRHI Depth Test", 960, 540)) {
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

    const std::array<Vertex, 8> vertices = {{
        {{-0.75f, 0.55f, 0.60f}, {0.20f, 0.35f, 0.95f, 1.0f}},
        {{-0.75f, -0.35f, 0.60f}, {0.20f, 0.35f, 0.95f, 1.0f}},
        {{0.15f, -0.35f, 0.60f}, {0.20f, 0.35f, 0.95f, 1.0f}},
        {{0.15f, 0.55f, 0.60f}, {0.20f, 0.35f, 0.95f, 1.0f}},
        {{-0.35f, 0.20f, -0.10f}, {0.95f, 0.35f, 0.25f, 1.0f}},
        {{-0.35f, -0.65f, -0.10f}, {0.95f, 0.35f, 0.25f, 1.0f}},
        {{0.55f, -0.65f, -0.10f}, {0.95f, 0.35f, 0.25f, 1.0f}},
        {{0.55f, 0.20f, -0.10f}, {0.95f, 0.35f, 0.25f, 1.0f}},
    }};

    const std::array<uint16_t, 12> indices = {{
        0,
        1,
        2,
        0,
        2,
        3,
        4,
        5,
        6,
        4,
        6,
        7,
    }};

    BufferHandle vertexBuffer = device->createBuffer(
        BufferDesc{.size = sizeof(vertices), .usage = BufferUsage::Vertex | BufferUsage::CopyDst}, vertices.data());
    BufferHandle indexBuffer = device->createBuffer(
        BufferDesc{.size = sizeof(indices), .usage = BufferUsage::Index | BufferUsage::CopyDst}, indices.data());
    ShaderHandle vertexShader = device->createShader(ShaderDesc{.stage = ShaderStage::Vertex, .source = kVertexShader});
    ShaderHandle fragmentShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Fragment, .source = kFragmentShader});
    PipelineLayoutHandle layout = device->createPipelineLayout(PipelineLayoutDesc{});

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
                            VertexAttributeDesc{
                                .location = 0,
                                .format = VertexFormat::Float3,
                                .offset = offsetof(Vertex, position),
                            },
                            VertexAttributeDesc{
                                .location = 3,
                                .format = VertexFormat::Float4,
                                .offset = offsetof(Vertex, color),
                            },
                        },
                },
            },
    };
    pipelineDesc.layout = layout;
    pipelineDesc.vertex_shader = vertexShader;
    pipelineDesc.fragment_shader = fragmentShader;
    pipelineDesc.render_target_state.color_targets.push_back(ColorTargetState{.format = TextureFormat::RGBA8_UNorm});
    pipelineDesc.depth_state.enabled = true;
    pipelineDesc.depth_state.write_enabled = true;
    pipelineDesc.depth_state.compare = CompareOp::Less;

    PipelineHandle pipeline = device->createPipeline(pipelineDesc);
    if (!vertexBuffer || !indexBuffer || !vertexShader || !fragmentShader || !layout || !pipeline) {
        std::printf("Failed to create depth test resources.\n");
        instance->shutdown();
        return 1;
    }

    while (surface.pollEvents() && !surface.shouldClose()) {
        swapchain->resize(surface.getWidth(), surface.getHeight());
        SwapchainFrame frame{};
        if (!device->beginFrame(swapchainHandle, frame)) {
            break;
        }

        RenderPassBeginInfo pass{};
        pass.color_attachments.push_back(ColorAttachmentDesc{
            .view = frame.color_view,
            .load_op = LoadOp::Clear,
            .store_op = StoreOp::Store,
            .clear_color = ClearColor{0.05f, 0.05f, 0.07f, 1.0f},
        });
        pass.has_depth_stencil_attachment = true;
        pass.depth_stencil_attachment = DepthStencilAttachmentDesc{
            .view = frame.depth_stencil_view,
            .depth_load_op = LoadOp::Clear,
            .depth_store_op = StoreOp::DontCare,
            .clear_depth = 1.0f,
        };
        pass.width = frame.width;
        pass.height = frame.height;

        commands.begin();
        commands.beginRenderPass(pass);
        commands.setPipeline(pipeline);
        commands.setVertexBuffer(0, vertexBuffer);
        commands.setIndexBuffer(indexBuffer, IndexFormat::UInt16);
        commands.drawIndexed(static_cast<uint32_t>(indices.size()));
        commands.endRenderPass();
        commands.end();

        device->submit(commandListHandle, &frame);
        device->present(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    device->destroyPipeline(pipeline);
    device->destroyPipelineLayout(layout);
    device->destroyShader(fragmentShader);
    device->destroyShader(vertexShader);
    device->destroyBuffer(indexBuffer);
    device->destroyBuffer(vertexBuffer);
    instance->shutdown();
    return 0;
}
