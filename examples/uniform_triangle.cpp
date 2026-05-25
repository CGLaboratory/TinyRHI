#include "TinyRHI/backend_factory.h"
#include "common/win32_gl_surface.h"

#include <chrono>
#include <cmath>
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

    tinyrhi_examples::Win32GLSurface surface;
    if (!surface.create("TinyRHI Uniform Triangle", 960, 540, instance->getWindowRequirements())) {
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

    const Vertex vertices[] = {
        {{0.0f, 0.65f, 0.0f}},
        {{-0.65f, -0.55f, 0.0f}},
        {{0.65f, -0.55f, 0.0f}},
    };

    UniformData params{{0.9f, 0.35f, 0.25f, 1.0f}};

    BufferHandle vertexBuffer = device->createBuffer(
        BufferDesc{.type = BufferType::VertexBuffer, .usage = BufferUsage::Static, .size = sizeof(vertices)},
        vertices);
    BufferHandle uniformBuffer = device->createBuffer(
        BufferDesc{.type = BufferType::UniformBuffer, .usage = BufferUsage::Dynamic, .size = sizeof(UniformData)},
        &params);
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
                    .offset = 0,
                },
            },
    };
    pipelineDesc.layout = layout;
    pipelineDesc.vertex_shader = vertexShader;
    pipelineDesc.fragment_shader = fragmentShader;
    pipelineDesc.render_target_state.color_targets.push_back(ColorTargetState{.format = TextureFormat::RGBA8});
    pipelineDesc.depth_state.enabled = false;

    PipelineHandle pipeline = device->createPipeline(pipelineDesc);
    if (vertexBuffer == 0 || uniformBuffer == 0 || vertexShader == 0 || fragmentShader == 0 || layout == 0 ||
        pipeline == 0) {
        std::printf("Failed to create uniform triangle resources.\n");
        instance->shutdown();
        return 1;
    }

    const auto start = std::chrono::steady_clock::now();

    while (surface.pollEvents() && !surface.shouldClose()) {
        swapchain->resize(surface.getWidth(), surface.getHeight());

        const auto elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
        params.color[0] = 0.5f + 0.5f * std::sin(elapsed * 1.7f);
        params.color[1] = 0.5f + 0.5f * std::sin(elapsed * 1.3f + 2.0f);
        params.color[2] = 0.5f + 0.5f * std::sin(elapsed * 0.9f + 4.0f);
        params.color[3] = 1.0f;
        device->updateBuffer(uniformBuffer, &params, sizeof(params));

        RenderPassBeginInfo pass{};
        pass.color_attachments.push_back(ColorAttachmentDesc{
            .view = swapchain->getCurrentColorTextureView(),
            .load_op = LoadOp::Clear,
            .store_op = StoreOp::Store,
            .clear_color = ClearColor{0.05f, 0.05f, 0.06f, 1.0f},
        });
        pass.width = swapchain->getWidth();
        pass.height = swapchain->getHeight();

        commands.begin();
        commands.beginRenderPass(pass);
        commands.setPipeline(pipeline);
        commands.setUniformBuffer(0, uniformBuffer);
        commands.setVertexBuffer(0, vertexBuffer);
        commands.draw(3);
        commands.endRenderPass();
        commands.end();

        swapchain->present();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    device->destroyPipeline(pipeline);
    device->destroyPipelineLayout(layout);
    device->destroyShader(fragmentShader);
    device->destroyShader(vertexShader);
    device->destroyBuffer(uniformBuffer);
    device->destroyBuffer(vertexBuffer);
    instance->shutdown();
    return 0;
}
