#include "TinyRHI/backend_factory.h"
#include "common/win32_window.h"

#include "stb_image.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

using namespace lunalite::rhi;

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Vertex {
    float position[3];
};

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct Mat4 {
    float m[16]{};
};

struct PushConstants {
    Mat4 view_projection;
};

constexpr std::array<Vertex, 36> kCubeVertices = {{
    {{-1.0f, 1.0f, -1.0f}},  {{-1.0f, -1.0f, -1.0f}}, {{1.0f, -1.0f, -1.0f}},
    {{1.0f, -1.0f, -1.0f}},  {{1.0f, 1.0f, -1.0f}},   {{-1.0f, 1.0f, -1.0f}},
    {{-1.0f, -1.0f, 1.0f}},  {{-1.0f, -1.0f, -1.0f}}, {{-1.0f, 1.0f, -1.0f}},
    {{-1.0f, 1.0f, -1.0f}},  {{-1.0f, 1.0f, 1.0f}},   {{-1.0f, -1.0f, 1.0f}},
    {{1.0f, -1.0f, -1.0f}},  {{1.0f, -1.0f, 1.0f}},   {{1.0f, 1.0f, 1.0f}},
    {{1.0f, 1.0f, 1.0f}},    {{1.0f, 1.0f, -1.0f}},   {{1.0f, -1.0f, -1.0f}},
    {{-1.0f, -1.0f, 1.0f}},  {{-1.0f, 1.0f, 1.0f}},   {{1.0f, 1.0f, 1.0f}},
    {{1.0f, 1.0f, 1.0f}},    {{1.0f, -1.0f, 1.0f}},   {{-1.0f, -1.0f, 1.0f}},
    {{-1.0f, 1.0f, -1.0f}},  {{1.0f, 1.0f, -1.0f}},   {{1.0f, 1.0f, 1.0f}},
    {{1.0f, 1.0f, 1.0f}},    {{-1.0f, 1.0f, 1.0f}},   {{-1.0f, 1.0f, -1.0f}},
    {{-1.0f, -1.0f, -1.0f}}, {{-1.0f, -1.0f, 1.0f}},  {{1.0f, -1.0f, -1.0f}},
    {{1.0f, -1.0f, -1.0f}},  {{-1.0f, -1.0f, 1.0f}},  {{1.0f, -1.0f, 1.0f}},
}};

constexpr const char* kVertexShader = R"GLSL(
#version 450 core
layout(location = 0) in vec3 inPosition;

uniform vec4 uPushConstants[4];

out vec3 vDirection;

void main()
{
    mat4 viewProjection = mat4(
        uPushConstants[0],
        uPushConstants[1],
        uPushConstants[2],
        uPushConstants[3]);
    vec4 position = viewProjection * vec4(inPosition, 1.0);
    gl_Position = position.xyww;
    vDirection = inPosition;
}
)GLSL";

constexpr const char* kFragmentShader = R"GLSL(
#version 450 core
layout(binding = 0) uniform samplerCube uEnvironment;

in vec3 vDirection;
out vec4 outColor;

void main()
{
    vec3 color = texture(uEnvironment, normalize(vDirection)).rgb;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
)GLSL";

Vec3 operator-(Vec3 lhs, Vec3 rhs)
{
    return Vec3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 cross(Vec3 lhs, Vec3 rhs)
{
    return Vec3{
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

float dot(Vec3 lhs, Vec3 rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 normalize(Vec3 value)
{
    const float length = std::sqrt(dot(value, value));
    if (length <= 0.0f) {
        return Vec3{};
    }

    return Vec3{value.x / length, value.y / length, value.z / length};
}

Mat4 multiply(const Mat4& lhs, const Mat4& rhs)
{
    Mat4 result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0f;
            for (int i = 0; i < 4; ++i) {
                value += lhs.m[i * 4 + row] * rhs.m[column * 4 + i];
            }
            result.m[column * 4 + row] = value;
        }
    }

    return result;
}

Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane)
{
    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4 result{};
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    return result;
}

Mat4 lookAtRotation(Vec3 eye, Vec3 center, Vec3 up)
{
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    Mat4 result{};
    result.m[0] = s.x;
    result.m[1] = u.x;
    result.m[2] = -f.x;
    result.m[4] = s.y;
    result.m[5] = u.y;
    result.m[6] = -f.y;
    result.m[8] = s.z;
    result.m[9] = u.z;
    result.m[10] = -f.z;
    result.m[15] = 1.0f;
    return result;
}

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

Vec3 cubeDirection(uint32_t face, uint32_t x, uint32_t y, uint32_t size)
{
    const float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size)) - 1.0f;
    const float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size)) - 1.0f;

    switch (face) {
        case 0:
            return normalize(Vec3{1.0f, -v, -u});
        case 1:
            return normalize(Vec3{-1.0f, -v, u});
        case 2:
            return normalize(Vec3{u, 1.0f, v});
        case 3:
            return normalize(Vec3{u, -1.0f, -v});
        case 4:
            return normalize(Vec3{u, -v, 1.0f});
        case 5:
            return normalize(Vec3{-u, -v, -1.0f});
    }

    return Vec3{0.0f, 0.0f, 1.0f};
}

void sampleEquirectangular(const float* hdr, int width, int height, Vec3 direction, float* outRgba)
{
    const float longitude = std::atan2(direction.z, direction.x);
    const float latitude = std::acos(std::clamp(direction.y, -1.0f, 1.0f));
    const float u = longitude / (2.0f * kPi) + 0.5f;
    const float v = latitude / kPi;

    const float px = u * static_cast<float>(width - 1);
    const float py = v * static_cast<float>(height - 1);
    const int x0 = std::clamp(static_cast<int>(std::floor(px)), 0, width - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(py)), 0, height - 1);
    const int x1 = (x0 + 1) % width;
    const int y1 = std::clamp(y0 + 1, 0, height - 1);
    const float tx = px - static_cast<float>(x0);
    const float ty = py - static_cast<float>(y0);

    const auto texel = [&](int x, int y) {
        return hdr + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
    };
    const float* c00 = texel(x0, y0);
    const float* c10 = texel(x1, y0);
    const float* c01 = texel(x0, y1);
    const float* c11 = texel(x1, y1);

    for (int channel = 0; channel < 3; ++channel) {
        const float top = c00[channel] * (1.0f - tx) + c10[channel] * tx;
        const float bottom = c01[channel] * (1.0f - tx) + c11[channel] * tx;
        outRgba[channel] = top * (1.0f - ty) + bottom * ty;
    }
    outRgba[3] = 1.0f;
}

std::vector<float> makeCubemapFace(const float* hdr, int width, int height, uint32_t face, uint32_t size)
{
    std::vector<float> pixels(static_cast<size_t>(size) * static_cast<size_t>(size) * 4);
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            sampleEquirectangular(
                hdr,
                width,
                height,
                cubeDirection(face, x, y, size),
                pixels.data() + (static_cast<size_t>(y) * static_cast<size_t>(size) + x) * 4);
        }
    }

    return pixels;
}

std::string findAssetPath(int argc, char** argv)
{
    const std::filesystem::path exeDir = argc > 0 ? std::filesystem::path(argv[0]).parent_path() : std::filesystem::path{};
    const std::array<std::filesystem::path, 3> paths = {
        exeDir / "newport_loft.hdr",
        "newport_loft.hdr",
        "examples/cube_map/newport_loft.hdr",
    };

    for (const auto& path : paths) {
        if (std::filesystem::exists(path)) {
            return path.string();
        }
    }

    return "newport_loft.hdr";
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
    if (!surface.create("TinyRHI Cube Map", 960, 540)) {
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

    const std::string assetPath = findAssetPath(argc, argv);
    int hdrWidth = 0;
    int hdrHeight = 0;
    float* hdrPixels = stbi_loadf(assetPath.c_str(), &hdrWidth, &hdrHeight, nullptr, 4);
    if (hdrPixels == nullptr || hdrWidth <= 0 || hdrHeight <= 0) {
        std::printf("Failed to load HDR image %s: %s\n", assetPath.c_str(), stbi_failure_reason());
        instance->shutdown();
        return 1;
    }

    constexpr uint32_t cubeSize = 512;
    const uint32_t mipLevels = calculateMipLevels(cubeSize, cubeSize);
    TextureHandle environment = device->createTexture(TextureDesc{
        .width = cubeSize,
        .height = cubeSize,
        .dimension = TextureDimension::TextureCube,
        .format = TextureFormat::RGBA32F,
        .usage = TextureUsage::Sampled | TextureUsage::CopyDst,
        .mip_levels = mipLevels,
        .array_layers = 6,
    });

    if (!environment) {
        std::printf("Failed to create environment cubemap.\n");
        stbi_image_free(hdrPixels);
        instance->shutdown();
        return 1;
    }

    for (uint32_t face = 0; face < 6; ++face) {
        const std::vector<float> facePixels = makeCubemapFace(hdrPixels, hdrWidth, hdrHeight, face, cubeSize);
        TextureUploadDesc upload{};
        upload.width = cubeSize;
        upload.height = cubeSize;
        upload.mip_level = 0;
        upload.array_layer = face;
        upload.format = TextureFormat::RGBA32F;
        upload.data = facePixels.data();
        upload.row_pitch = static_cast<size_t>(cubeSize) * 4 * sizeof(float);
        device->updateTexture(environment, upload);
    }
    stbi_image_free(hdrPixels);
    device->generateMipmaps(environment);

    TextureViewHandle environmentView = device->createTextureView(TextureViewDesc{
        .texture = environment,
        .view_dimension = TextureViewDimension::TextureCube,
        .format = TextureFormat::RGBA32F,
        .aspect = TextureAspect::Color,
        .mip_level_count = mipLevels,
        .array_layer_count = 6,
    });
    SamplerHandle environmentSampler = device->createSampler(SamplerDesc{
        .min_filter = FilterMode::Linear,
        .mag_filter = FilterMode::Linear,
        .mip_filter = MipFilter::Linear,
        .address_u = AddressMode::ClampToEdge,
        .address_v = AddressMode::ClampToEdge,
        .address_w = AddressMode::ClampToEdge,
    });

    BufferHandle vertexBuffer = device->createBuffer(
        BufferDesc{.size = sizeof(kCubeVertices), .usage = BufferUsage::Vertex | BufferUsage::CopyDst},
        kCubeVertices.data());
    ShaderHandle vertexShader = device->createShader(ShaderDesc{.stage = ShaderStage::Vertex, .source = kVertexShader});
    ShaderHandle fragmentShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Fragment, .source = kFragmentShader});

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
    pipelineLayoutDesc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Vertex),
        .offset = 0,
        .size = sizeof(PushConstants),
    });
    PipelineLayoutHandle layout = device->createPipelineLayout(pipelineLayoutDesc);

    BindGroupDesc bindGroupDesc{};
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .texture_view = environmentView,
        .sampler = environmentSampler,
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
    pipelineDesc.render_target_state.color_targets.push_back(ColorTargetState{.format = TextureFormat::RGBA8_UNorm});
    pipelineDesc.depth_state.enabled = false;
    pipelineDesc.raster_state.cull_mode = CullMode::None;

    PipelineHandle pipeline = device->createPipeline(pipelineDesc);
    if (!environmentView || !environmentSampler || !vertexBuffer || !vertexShader || !fragmentShader || !bindGroupLayout ||
        !layout || !bindGroup || !pipeline) {
        std::printf("Failed to create cubemap example resources.\n");
        instance->shutdown();
        return 1;
    }

    auto& commands = device->getCommandList();
    const auto start = std::chrono::steady_clock::now();

    while (surface.pollEvents() && !surface.shouldClose()) {
        swapchain->resize(surface.getWidth(), surface.getHeight());

        const auto elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
        const float aspect = swapchain->getHeight() > 0
                                 ? static_cast<float>(swapchain->getWidth()) / static_cast<float>(swapchain->getHeight())
                                 : 1.0f;
        const Vec3 eye{std::sin(elapsed * 0.2f), 0.15f, std::cos(elapsed * 0.2f)};
        const Mat4 projection = perspective(70.0f * kPi / 180.0f, aspect, 0.1f, 10.0f);
        const Mat4 view = lookAtRotation(eye, Vec3{}, Vec3{0.0f, 1.0f, 0.0f});
        const PushConstants constants{multiply(projection, view)};

        RenderPassBeginInfo pass{};
        pass.color_attachments.push_back(ColorAttachmentDesc{
            .view = swapchain->getCurrentColorTextureView(),
            .load_op = LoadOp::Clear,
            .store_op = StoreOp::Store,
            .clear_color = ClearColor{0.0f, 0.0f, 0.0f, 1.0f},
        });
        pass.width = swapchain->getWidth();
        pass.height = swapchain->getHeight();

        commands.begin();
        commands.beginRenderPass(pass);
        commands.setPipeline(pipeline);
        commands.setBindGroup(0, bindGroup);
        commands.pushConstants(shaderStageFlag(ShaderStage::Vertex), 0, sizeof(constants), &constants);
        commands.setVertexBuffer(0, vertexBuffer);
        commands.draw(static_cast<uint32_t>(kCubeVertices.size()));
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
    device->destroyBuffer(vertexBuffer);
    device->destroySampler(environmentSampler);
    device->destroyTextureView(environmentView);
    device->destroyTexture(environment);
    instance->shutdown();
    return 0;
}
