#include "common/win32_window.h"
#include "TinyRHI/backend_factory.h"

#include <cstdio>

#include <chrono>
#include <thread>

using namespace lunalite::rhi;

namespace {

constexpr uint32_t kLutSize = 512;
constexpr uint32_t kComputeGroupSize = 8;

constexpr const char* kComputeShader = R"GLSL(
#version 450 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rg16f, binding = 0) writeonly uniform image2D uBRDFLut;

const float PI = 3.14159265359;
const uint SAMPLE_COUNT = 1024u;

float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint n)
{
    return vec2(float(i) / float(n), RadicalInverseVdC(i));
}

vec3 ImportanceSampleGGX(vec2 xi, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));

    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float GeometrySchlickGGX(float nDotV, float roughness)
{
    float a = roughness;
    float k = (a * a) * 0.5;
    float denom = nDotV * (1.0 - k) + k;
    return nDotV / denom;
}

float GeometrySmith(float nDotV, float nDotL, float roughness)
{
    return GeometrySchlickGGX(nDotV, roughness) * GeometrySchlickGGX(nDotL, roughness);
}

vec2 IntegrateBRDF(float nDotV, float roughness)
{
    vec3 view = vec3(sqrt(max(1.0 - nDotV * nDotV, 0.0)), 0.0, nDotV);

    float scale = 0.0;
    float bias = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 xi = Hammersley(i, SAMPLE_COUNT);
        vec3 halfVector = ImportanceSampleGGX(xi, roughness);
        vec3 light = normalize(2.0 * dot(view, halfVector) * halfVector - view);

        float nDotL = max(light.z, 0.0);
        float nDotH = max(halfVector.z, 0.0);
        float vDotH = max(dot(view, halfVector), 0.0);

        if (nDotL > 0.0) {
            float geometry = GeometrySmith(nDotV, nDotL, roughness);
            float geometryVisibility = (geometry * vDotH) / max(nDotH * nDotV, 0.001);
            float fresnel = pow(1.0 - vDotH, 5.0);

            scale += (1.0 - fresnel) * geometryVisibility;
            bias += fresnel * geometryVisibility;
        }
    }

    return vec2(scale, bias) / float(SAMPLE_COUNT);
}

void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(uBRDFLut);
    if (pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }

    vec2 uv = (vec2(pixel) + vec2(0.5)) / vec2(size);
    float nDotV = max(uv.x, 0.001);
    float roughness = max(uv.y, 0.001);
    vec2 integrated = IntegrateBRDF(nDotV, roughness);

    imageStore(uBRDFLut, pixel, vec4(integrated, 0.0, 1.0));
}
)GLSL";

constexpr const char* kVertexShader = R"GLSL(
#version 450 core
out vec2 uv;

void main()
{
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0));

    vec2 position = positions[gl_VertexID];
    uv = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)GLSL";

constexpr const char* kFragmentShader = R"GLSL(
#version 450 core
layout(binding = 0) uniform sampler2D uBRDFLut;

in vec2 uv;
out vec4 outColor;

void main()
{
    vec2 brdf = texture(uBRDFLut, uv).rg;
    outColor = vec4(brdf.x, brdf.y, brdf.x + brdf.y, 1.0);
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
    if (!surface.create("TinyRHI BRDF LUT", 960, 540)) {
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

    TextureHandle brdfLut = device->createTexture(TextureDesc{
        .width = kLutSize,
        .height = kLutSize,
        .dimension = TextureDimension::Texture2D,
        .format = TextureFormat::RG16F,
        .usage = TextureUsage::Sampled | TextureUsage::Storage,
        .mip_levels = 1,
        .array_layers = 1,
    });
    TextureViewHandle brdfLutView = device->createTextureView(TextureViewDesc{
        .texture = brdfLut,
        .view_dimension = TextureViewDimension::Texture2D,
        .format = TextureFormat::RG16F,
        .aspect = TextureAspect::Color,
        .mip_level_count = 1,
        .array_layer_count = 1,
    });
    SamplerHandle lutSampler = device->createSampler(SamplerDesc{
        .min_filter = FilterMode::Linear,
        .mag_filter = FilterMode::Linear,
        .mip_filter = MipFilter::None,
        .address_u = AddressMode::ClampToEdge,
        .address_v = AddressMode::ClampToEdge,
        .address_w = AddressMode::ClampToEdge,
    });

    ShaderHandle computeShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Compute, .source = kComputeShader});
    ShaderHandle vertexShader = device->createShader(ShaderDesc{.stage = ShaderStage::Vertex, .source = kVertexShader});
    ShaderHandle fragmentShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Fragment, .source = kFragmentShader});

    BindGroupLayoutDesc computeBindGroupLayoutDesc{};
    computeBindGroupLayoutDesc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::StorageTexture,
        .stages = shaderStageFlag(ShaderStage::Compute),
        .count = 1,
    });
    BindGroupLayoutHandle computeBindGroupLayout = device->createBindGroupLayout(computeBindGroupLayoutDesc);

    PipelineLayoutDesc computeLayoutDesc{};
    computeLayoutDesc.bind_group_layouts.push_back(computeBindGroupLayout);
    PipelineLayoutHandle computeLayout = device->createPipelineLayout(computeLayoutDesc);

    BindGroupDesc computeBindGroupDesc{};
    computeBindGroupDesc.layout = computeBindGroupLayout;
    computeBindGroupDesc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::StorageTexture,
        .texture_view = brdfLutView,
    });
    BindGroupHandle computeBindGroup = device->createBindGroup(computeBindGroupDesc);

    PipelineHandle computePipeline = device->createComputePipeline(ComputePipelineDesc{
        .layout = computeLayout,
        .compute_shader = computeShader,
    });

    BindGroupLayoutDesc graphicsBindGroupLayoutDesc{};
    graphicsBindGroupLayoutDesc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .count = 1,
    });
    BindGroupLayoutHandle graphicsBindGroupLayout = device->createBindGroupLayout(graphicsBindGroupLayoutDesc);

    PipelineLayoutDesc graphicsLayoutDesc{};
    graphicsLayoutDesc.bind_group_layouts.push_back(graphicsBindGroupLayout);
    PipelineLayoutHandle graphicsLayout = device->createPipelineLayout(graphicsLayoutDesc);

    BindGroupDesc graphicsBindGroupDesc{};
    graphicsBindGroupDesc.layout = graphicsBindGroupLayout;
    graphicsBindGroupDesc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .texture_view = brdfLutView,
        .sampler = lutSampler,
    });
    BindGroupHandle graphicsBindGroup = device->createBindGroup(graphicsBindGroupDesc);

    PipelineDesc graphicsDesc{};
    graphicsDesc.topology = PrimitiveTopology::Triangle;
    graphicsDesc.layout = graphicsLayout;
    graphicsDesc.vertex_shader = vertexShader;
    graphicsDesc.fragment_shader = fragmentShader;
    graphicsDesc.render_target_state.color_targets.push_back(ColorTargetState{.format = TextureFormat::RGBA8_UNorm});
    graphicsDesc.depth_state.enabled = false;
    PipelineHandle graphicsPipeline = device->createPipeline(graphicsDesc);

    if (!brdfLut || !brdfLutView || !lutSampler || !computeShader || !vertexShader || !fragmentShader ||
        !computeBindGroupLayout || !computeLayout || !computeBindGroup || !computePipeline ||
        !graphicsBindGroupLayout || !graphicsLayout || !graphicsBindGroup || !graphicsPipeline) {
        std::printf("Failed to create BRDF LUT resources.\n");
        instance->shutdown();
        return 1;
    }

    commands.begin();
    commands.setPipeline(computePipeline);
    commands.setBindGroup(0, computeBindGroup);
    commands.dispatch((kLutSize + kComputeGroupSize - 1) / kComputeGroupSize,
                      (kLutSize + kComputeGroupSize - 1) / kComputeGroupSize);
    TextureBarrier brdfLutBarrier{
        .texture = brdfLut,
        .old_state = ResourceState::StorageWrite,
        .new_state = ResourceState::ShaderRead,
    };
    commands.resourceBarrier(&brdfLutBarrier, 1);
    commands.end();
    device->submit(commandListHandle);

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
            .clear_color = ClearColor{0.02f, 0.025f, 0.03f, 1.0f},
        });
        pass.width = frame.width;
        pass.height = frame.height;

        commands.begin();
        commands.beginRenderPass(pass);
        commands.setPipeline(graphicsPipeline);
        commands.setBindGroup(0, graphicsBindGroup);
        commands.draw(3);
        commands.endRenderPass();
        commands.end();

        device->submit(commandListHandle, &frame);
        device->present(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    device->destroyPipeline(graphicsPipeline);
    device->destroyBindGroup(graphicsBindGroup);
    device->destroyPipelineLayout(graphicsLayout);
    device->destroyBindGroupLayout(graphicsBindGroupLayout);
    device->destroyPipeline(computePipeline);
    device->destroyBindGroup(computeBindGroup);
    device->destroyPipelineLayout(computeLayout);
    device->destroyBindGroupLayout(computeBindGroupLayout);
    device->destroyShader(fragmentShader);
    device->destroyShader(vertexShader);
    device->destroyShader(computeShader);
    device->destroySampler(lutSampler);
    device->destroyTextureView(brdfLutView);
    device->destroyTexture(brdfLut);
    instance->shutdown();
    return 0;
}
