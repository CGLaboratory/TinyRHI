#include "TinyRHI/backend_factory.h"
#include "common/win32_window.h"

#define STB_IMAGE_IMPLEMENTATION
#include "common/stb/stb_image.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>

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

uint32_t calculateMipLevels(uint32_t width, uint32_t height)
{
    uint32_t levels = 1;
    while (width > 1 || height > 1) {
        width = width > 1 ? width / 2 : 1;
        height = height > 1 ? height / 2 : 1;
        ++levels;
    }

    return levels;
}

} // namespace

int main(int argc, char** argv)
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
    const SwapchainHandle swapchainHandle =
        device->createSwapchain(surfaceHandle, SwapchainDesc{.color_format = TextureFormat::RGBA8_SRGB});
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

    int imageWidth = 0;
    int imageHeight = 0;
    stbi_uc* imagePixels = nullptr;
    const std::filesystem::path exeDir = argc > 0 ? std::filesystem::path(argv[0]).parent_path() : std::filesystem::path{};
    const std::array<std::filesystem::path, 3> imagePaths = {
        exeDir / "test.png",
        "test.png",
        "examples/textured_quad/test.png",
    };
    std::string loadedImagePath;
    for (const auto& imagePath : imagePaths) {
        loadedImagePath = imagePath.string();
        imagePixels = stbi_load(loadedImagePath.c_str(), &imageWidth, &imageHeight, nullptr, STBI_rgb_alpha);
        if (imagePixels != nullptr) {
            break;
        }
    }

    if (imagePixels == nullptr || imageWidth <= 0 || imageHeight <= 0) {
        std::printf("Failed to load texture image test.png: %s\n", stbi_failure_reason());
        instance->shutdown();
        return 1;
    }

    const auto textureWidth = static_cast<uint32_t>(imageWidth);
    const auto textureHeight = static_cast<uint32_t>(imageHeight);
    const uint32_t mipLevels = calculateMipLevels(textureWidth, textureHeight);

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
        .format = TextureFormat::RGBA8_SRGB,
        .usage = TextureUsage::Sampled | TextureUsage::CopyDst,
        .mip_levels = mipLevels,
    });
    TextureViewHandle textureView = device->createTextureView(TextureViewDesc{
        .texture = texture,
        .format = TextureFormat::RGBA8_SRGB,
        .aspect = TextureAspect::Color,
        .mip_level_count = mipLevels,
    });
    SamplerHandle sampler = device->createSampler(SamplerDesc{
        .min_filter = FilterMode::Linear,
        .mag_filter = FilterMode::Linear,
        .mip_filter = MipFilter::Linear,
        .address_u = AddressMode::Repeat,
        .address_v = AddressMode::MirroredRepeat,
        .address_w = AddressMode::ClampToEdge,
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
    uploadDesc.mip_level = 0;
    uploadDesc.format = TextureFormat::RGBA8_SRGB;
    uploadDesc.data = imagePixels;
    uploadDesc.row_pitch = textureWidth * 4;
    device->updateTexture(texture, uploadDesc);
    device->generateMipmaps(texture);
    stbi_image_free(imagePixels);

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
    pipelineDesc.render_target_state.color_targets.push_back(ColorTargetState{.format = TextureFormat::RGBA8_SRGB});
    pipelineDesc.depth_state.enabled = false;

    PipelineHandle pipeline = device->createPipeline(pipelineDesc);
    if (!vertexBuffer || !indexBuffer || !vertexShader || !fragmentShader || !texture || !textureView || !sampler
        || !bindGroupLayout || !layout || !bindGroup || !pipeline) {
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
