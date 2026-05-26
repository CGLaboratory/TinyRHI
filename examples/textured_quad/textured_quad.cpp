#include "TinyRHI/backend_factory.h"
#include "common/win32_window.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <thread>
#include <vector>

using namespace lunalite::rhi;

namespace {

struct Vertex {
    float position[3];
    float uv[2];
};

constexpr const char* kVertexShader = R"GLSL(
#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inUV;

out vec2 vUV;

void main()
{
    gl_Position = vec4(inPosition, 1.0);
    vUV = inUV;
}
)GLSL";

constexpr const char* kFragmentShader = R"GLSL(
#version 450 core
layout(binding = 0) uniform sampler2D uTexture;

in vec2 vUV;
out vec4 outColor;

void main()
{
    outColor = texture(uTexture, vUV);
}
)GLSL";

uint32_t packRgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16)
           | (static_cast<uint32_t>(a) << 24);
}

} // namespace

int main()
{
    auto instance = BackendFactory::createInstance(BackendType::OpenGL);
    if (!instance) {
        std::printf("Failed to create OpenGL backend.\n");
        return 1;
    }

    tinyrhi_examples::Win32Window surface;
    if (!surface.create("TinyRHI Textured Quad", 960, 540)) {
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

    const std::array<Vertex, 4> vertices = {{
        {{-0.85f, 0.85f, 0.0f}, {0.0f, 0.0f}},
        {{-0.85f, -0.85f, 0.0f}, {0.0f, 1.0f}},
        {{0.85f, -0.85f, 0.0f}, {1.0f, 1.0f}},
        {{0.85f, 0.85f, 0.0f}, {1.0f, 0.0f}},
    }};

    const std::array<uint16_t, 6> indices = {{0, 1, 2, 0, 2, 3}};

    constexpr uint32_t textureWidth = 128;
    constexpr uint32_t textureHeight = 128;
    std::vector<uint32_t> texturePixels(textureWidth * textureHeight);
    for (uint32_t y = 0; y < textureHeight; ++y) {
        for (uint32_t x = 0; x < textureWidth; ++x) {
            const bool light = ((x / 16) + (y / 16)) % 2 == 0;
            const uint8_t r = light ? 240 : 35;
            const uint8_t g = light ? 230 : 55;
            const uint8_t b = light ? 80 : 140;
            texturePixels[y * textureWidth + x] = packRgba(r, g, b, 255);
        }
    }

    BufferHandle vertexBuffer = device->createBuffer(
        BufferDesc{.size = sizeof(vertices), .usage = BufferUsage::Vertex | BufferUsage::CopyDst},
        vertices.data());
    BufferHandle indexBuffer = device->createBuffer(
        BufferDesc{.size = sizeof(indices), .usage = BufferUsage::Index | BufferUsage::CopyDst},
        indices.data());
    ShaderHandle vertexShader = device->createShader(ShaderDesc{.stage = ShaderStage::Vertex, .source = kVertexShader});
    ShaderHandle fragmentShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Fragment, .source = kFragmentShader});

    TextureHandle texture = device->createTexture(TextureDesc{
        .width = textureWidth,
        .height = textureHeight,
        .format = TextureFormat::RGBA8,
        .usage = TextureUsage::Sampled | TextureUsage::CopyDst,
    });
    TextureViewHandle textureView = device->createTextureView(TextureViewDesc{
        .texture = texture,
        .format = TextureFormat::RGBA8,
        .aspect = TextureAspect::Color,
    });
    SamplerHandle sampler = device->createSampler(SamplerDesc{
        .min_filter = FilterMode::Linear,
        .mag_filter = FilterMode::Linear,
        .address_u = AddressMode::Repeat,
        .address_v = AddressMode::Repeat,
    });

    BindGroupLayoutDesc bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .count = 1,
    });
    BindGroupLayoutHandle bindGroupLayout = device->createBindGroupLayout(bindGroupLayoutDesc);

    PipelineLayoutDesc pipelineLayoutDesc{};
    pipelineLayoutDesc.bind_group_layouts.push_back(bindGroupLayout);
    PipelineLayoutHandle layout = device->createPipelineLayout(pipelineLayoutDesc);

    BindGroupDesc bindGroupDesc{};
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .buffer = {},
        .texture_view = textureView,
        .sampler = sampler,
    });
    BindGroupHandle bindGroup = device->createBindGroup(bindGroupDesc);

    TextureUploadDesc uploadDesc{};
    uploadDesc.width = textureWidth;
    uploadDesc.height = textureHeight;
    uploadDesc.format = TextureFormat::RGBA8;
    uploadDesc.data = texturePixels.data();
    uploadDesc.row_pitch = textureWidth * sizeof(uint32_t);
    device->updateTexture(texture, uploadDesc);

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
                                .location = 2,
                                .format = VertexFormat::Float2,
                                .offset = offsetof(Vertex, uv),
                            },
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
    if (vertexBuffer == 0 || indexBuffer == 0 || vertexShader == 0 || fragmentShader == 0 || texture == 0
        || textureView == 0 || sampler == 0 || bindGroupLayout == 0 || layout == 0 || bindGroup == 0
        || pipeline == 0) {
        std::printf("Failed to create textured quad resources.\n");
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
            .clear_color = ClearColor{0.06f, 0.07f, 0.09f, 1.0f},
        });
        pass.width = swapchain->getWidth();
        pass.height = swapchain->getHeight();

        commands.begin();
        commands.beginRenderPass(pass);
        commands.setPipeline(pipeline);
        commands.setBindGroup(0, bindGroup);
        commands.setVertexBuffer(0, vertexBuffer);
        commands.setIndexBuffer(indexBuffer, IndexFormat::UInt16);
        commands.drawIndexed(static_cast<uint32_t>(indices.size()));
        commands.endRenderPass();
        commands.end();

        swapchain->present();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    device->destroyPipeline(pipeline);
    device->destroyBindGroup(bindGroup);
    device->destroyPipelineLayout(layout);
    device->destroyBindGroupLayout(bindGroupLayout);
    device->destroySampler(sampler);
    device->destroyTextureView(textureView);
    device->destroyTexture(texture);
    device->destroyShader(fragmentShader);
    device->destroyShader(vertexShader);
    device->destroyBuffer(indexBuffer);
    device->destroyBuffer(vertexBuffer);
    instance->shutdown();
    return 0;
}
