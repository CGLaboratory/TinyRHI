#include "TinyRHI/backend_factory.h"
#include "common/win32_gl_surface.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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

    tinyrhi_examples::Win32GLSurface surface;
    if (!surface.create("TinyRHI Alpha Blend", 960, 540, instance->getWindowRequirements())) {
        std::printf("Failed to create Win32 OpenGL surface.\n");
        return 1;
    }

    if (!instance->init(surface)) {
        std::printf("Failed to initialize TinyRHI instance.\n");
        return 1;
    }

    auto* device = instance->getDevice();
    auto* swapchain = instance->getSwapchain();
    auto& commands = device->getCommandList();

    constexpr std::array<Vertex, 12> vertices = {{
        {{-0.72f, 0.46f, 0.0f}, {0.95f, 0.22f, 0.16f, 0.68f}},
        {{-0.72f, -0.58f, 0.0f}, {0.95f, 0.22f, 0.16f, 0.68f}},
        {{0.28f, -0.58f, 0.0f}, {0.95f, 0.22f, 0.16f, 0.68f}},
        {{-0.72f, 0.46f, 0.0f}, {0.95f, 0.22f, 0.16f, 0.68f}},
        {{0.28f, -0.58f, 0.0f}, {0.95f, 0.22f, 0.16f, 0.68f}},
        {{0.28f, 0.46f, 0.0f}, {0.95f, 0.22f, 0.16f, 0.68f}},

        {{-0.24f, 0.66f, 0.0f}, {0.16f, 0.55f, 1.0f, 0.62f}},
        {{-0.24f, -0.34f, 0.0f}, {0.16f, 0.55f, 1.0f, 0.62f}},
        {{0.76f, -0.34f, 0.0f}, {0.16f, 0.55f, 1.0f, 0.62f}},
        {{-0.24f, 0.66f, 0.0f}, {0.16f, 0.55f, 1.0f, 0.62f}},
        {{0.76f, -0.34f, 0.0f}, {0.16f, 0.55f, 1.0f, 0.62f}},
        {{0.76f, 0.66f, 0.0f}, {0.16f, 0.55f, 1.0f, 0.62f}},
    }};

    BufferHandle vertexBuffer = device->createBuffer(
        BufferDesc{.type = BufferType::VertexBuffer, .usage = BufferUsage::Static, .size = sizeof(vertices)},
        vertices.data());
    ShaderHandle vertexShader = device->createShader(ShaderDesc{.stage = ShaderStage::Vertex, .source = kVertexShader});
    ShaderHandle fragmentShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Fragment, .source = kFragmentShader});
    PipelineLayoutHandle layout = device->createPipelineLayout(PipelineLayoutDesc{});

    PipelineDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Triangle;
    pipelineDesc.vertex_layout = VertexLayoutDesc{
        .stride = sizeof(Vertex),
        .attributes =
            {
                VertexAttributeDesc{
                    .semantic = VertexAttribute::Position,
                    .format = VertexFormat::Float3,
                    .offset = offsetof(Vertex, position),
                },
                VertexAttributeDesc{
                    .semantic = VertexAttribute::Color,
                    .format = VertexFormat::Float4,
                    .offset = offsetof(Vertex, color),
                },
            },
    };
    pipelineDesc.layout = layout;
    pipelineDesc.vertex_shader = vertexShader;
    pipelineDesc.fragment_shader = fragmentShader;
    ColorTargetState colorTarget{.format = TextureFormat::RGBA8};
    colorTarget.blend.enabled = true;
    pipelineDesc.render_target_state.color_targets.push_back(colorTarget);
    pipelineDesc.depth_state.enabled = false;

    PipelineHandle pipeline = device->createPipeline(pipelineDesc);
    if (vertexBuffer == 0 || vertexShader == 0 || fragmentShader == 0 || layout == 0 || pipeline == 0) {
        std::printf("Failed to create alpha blend resources.\n");
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
