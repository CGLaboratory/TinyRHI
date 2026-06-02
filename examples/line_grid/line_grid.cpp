#include "TinyRHI/backend_factory.h"
#include "common/win32_window.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

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

out vec4 vertexColor;

void main()
{
    gl_Position = vec4(inPosition, 1.0);
    vertexColor = inColor;
}
)GLSL";

constexpr const char* kFragmentShader = R"GLSL(
#version 450 core
in vec4 vertexColor;
out vec4 outColor;

void main()
{
    outColor = vertexColor;
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
    if (!surface.create("TinyRHI Line Grid", 960, 540)) {
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

    auto& commands = device->getCommandList();

    std::vector<Vertex> vertices;
    vertices.reserve(68);
    for (int i = -8; i <= 8; ++i) {
        const float p = static_cast<float>(i) / 8.0f * 0.82f;
        const float gridColor[4] = {0.34f, 0.37f, 0.42f, 1.0f};
        const float xAxisColor[4] = {0.95f, 0.34f, 0.28f, 1.0f};
        const float yAxisColor[4] = {0.24f, 0.58f, 0.95f, 1.0f};

        const float* horizontalColor = i == 0 ? xAxisColor : gridColor;
        vertices.push_back(Vertex{{-0.82f, p, 0.0f},
                                  {horizontalColor[0], horizontalColor[1], horizontalColor[2], horizontalColor[3]}});
        vertices.push_back(Vertex{{0.82f, p, 0.0f},
                                  {horizontalColor[0], horizontalColor[1], horizontalColor[2], horizontalColor[3]}});

        const float* verticalColor = i == 0 ? yAxisColor : gridColor;
        vertices.push_back(Vertex{{p, -0.82f, 0.0f},
                                  {verticalColor[0], verticalColor[1], verticalColor[2], verticalColor[3]}});
        vertices.push_back(Vertex{{p, 0.82f, 0.0f},
                                  {verticalColor[0], verticalColor[1], verticalColor[2], verticalColor[3]}});
    }

    BufferHandle vertexBuffer = device->createBuffer(
        BufferDesc{.size = vertices.size() * sizeof(Vertex), .usage = BufferUsage::Vertex | BufferUsage::CopyDst},
        vertices.data());
    ShaderHandle vertexShader = device->createShader(ShaderDesc{.stage = ShaderStage::Vertex, .source = kVertexShader});
    ShaderHandle fragmentShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Fragment, .source = kFragmentShader});
    PipelineLayoutHandle layout = device->createPipelineLayout(PipelineLayoutDesc{});

    PipelineDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Line;
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
    pipelineDesc.depth_state.enabled = false;

    PipelineHandle pipeline = device->createPipeline(pipelineDesc);
    if (!vertexBuffer || !vertexShader || !fragmentShader || !layout || !pipeline) {
        std::printf("Failed to create line grid resources.\n");
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
            .clear_color = ClearColor{0.045f, 0.048f, 0.055f, 1.0f},
        });
        pass.width = swapchain->getWidth();
        pass.height = swapchain->getHeight();

        commands.begin();
        commands.beginRenderPass(pass);
        commands.setPipeline(pipeline);
        commands.setVertexBuffer(0, vertexBuffer);
        commands.draw(static_cast<uint32_t>(vertices.size()));
        commands.endRenderPass();
        commands.end();

        swapchain->present();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    device->destroyPipeline(pipeline);
    device->destroyPipelineLayout(layout);
    device->destroyShader(fragmentShader);
    device->destroyShader(vertexShader);
    device->destroyBuffer(vertexBuffer);
    instance->shutdown();
    return 0;
}
