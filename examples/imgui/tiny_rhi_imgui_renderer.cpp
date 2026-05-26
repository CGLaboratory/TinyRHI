#include "tiny_rhi_imgui_renderer.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

using namespace lunalite::rhi;

namespace tinyrhi_examples {
namespace {

constexpr const char* kVertexShader = R"GLSL(
#version 450 core
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

uniform vec4 uPushConstants[4];

out vec2 vertexUV;
out vec4 vertexColor;

void main()
{
    mat4 projection = mat4(uPushConstants[0], uPushConstants[1], uPushConstants[2], uPushConstants[3]);
    gl_Position = projection * vec4(inPosition, 0.0, 1.0);
    vertexUV = inUV;
    vertexColor = inColor;
}
)GLSL";

constexpr const char* kFragmentShader = R"GLSL(
#version 450 core
layout(binding = 0) uniform sampler2D uTexture;

in vec2 vertexUV;
in vec4 vertexColor;
out vec4 outColor;

void main()
{
    outColor = vertexColor * texture(uTexture, vertexUV);
}
)GLSL";

constexpr size_t kInitialVertexBufferSize = 5000 * sizeof(ImDrawVert);
constexpr size_t kInitialIndexBufferSize = 10000 * sizeof(ImDrawIdx);
constexpr uint32_t kProjectionMatrixSize = 16 * sizeof(float);

ImTextureID textureIdFromBindGroup(BindGroupHandle bind_group)
{
    return static_cast<ImTextureID>(bind_group);
}

IndexFormat imguiIndexFormat()
{
    static_assert(sizeof(ImDrawIdx) == 2 || sizeof(ImDrawIdx) == 4);
    return sizeof(ImDrawIdx) == 2 ? IndexFormat::UInt16 : IndexFormat::UInt32;
}

} // namespace

TinyRHIImGuiRenderer::~TinyRHIImGuiRenderer()
{
    shutdown();
}

bool TinyRHIImGuiRenderer::init(Device& device)
{
    if (m_device != nullptr) {
        return false;
    }

    m_device = &device;
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "TinyRHI";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    if (!createStaticResources() || !createFontTexture()) {
        shutdown();
        return false;
    }

    return true;
}

void TinyRHIImGuiRenderer::shutdown()
{
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        io.BackendRendererName = nullptr;
        io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
        io.Fonts->SetTexID(ImTextureID_Invalid);
    }

    if (m_device == nullptr) {
        return;
    }

    destroyBuffers();
    destroyFontTexture();

    if (m_pipeline != 0) {
        m_device->destroyPipeline(m_pipeline);
        m_pipeline = 0;
    }
    if (m_pipeline_layout != 0) {
        m_device->destroyPipelineLayout(m_pipeline_layout);
        m_pipeline_layout = 0;
    }
    if (m_bind_group_layout != 0) {
        m_device->destroyBindGroupLayout(m_bind_group_layout);
        m_bind_group_layout = 0;
    }
    if (m_sampler != 0) {
        m_device->destroySampler(m_sampler);
        m_sampler = 0;
    }
    if (m_fragment_shader != 0) {
        m_device->destroyShader(m_fragment_shader);
        m_fragment_shader = 0;
    }
    if (m_vertex_shader != 0) {
        m_device->destroyShader(m_vertex_shader);
        m_vertex_shader = 0;
    }

    m_device = nullptr;
}

void TinyRHIImGuiRenderer::render(ImDrawData* draw_data, CommandList& commands)
{
    if (m_device == nullptr || draw_data == nullptr || draw_data->TotalVtxCount == 0) {
        return;
    }

    const int framebufferWidth =
        static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    const int framebufferHeight =
        static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }

    if (!ensureBuffers(draw_data->TotalVtxCount, draw_data->TotalIdxCount)) {
        return;
    }

    m_vertex_upload.resize(static_cast<size_t>(draw_data->TotalVtxCount));
    m_index_upload.resize(static_cast<size_t>(draw_data->TotalIdxCount));

    ImDrawVert* vertexDst = m_vertex_upload.data();
    ImDrawIdx* indexDst = m_index_upload.data();
    for (int listIndex = 0; listIndex < draw_data->CmdListsCount; ++listIndex) {
        const ImDrawList* commandList = draw_data->CmdLists[listIndex];
        const size_t vertexBytes = static_cast<size_t>(commandList->VtxBuffer.Size) * sizeof(ImDrawVert);
        const size_t indexBytes = static_cast<size_t>(commandList->IdxBuffer.Size) * sizeof(ImDrawIdx);
        std::memcpy(vertexDst, commandList->VtxBuffer.Data, vertexBytes);
        std::memcpy(indexDst, commandList->IdxBuffer.Data, indexBytes);
        vertexDst += commandList->VtxBuffer.Size;
        indexDst += commandList->IdxBuffer.Size;
    }

    m_device->updateBuffer(m_vertex_buffer, 0, m_vertex_upload.data(), m_vertex_upload.size() * sizeof(ImDrawVert));
    m_device->updateBuffer(m_index_buffer, 0, m_index_upload.data(), m_index_upload.size() * sizeof(ImDrawIdx));

    const float left = draw_data->DisplayPos.x;
    const float right = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    const float top = draw_data->DisplayPos.y;
    const float bottom = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    const float projection[4][4] = {
        {2.0f / (right - left), 0.0f, 0.0f, 0.0f},
        {0.0f, 2.0f / (top - bottom), 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f, 0.0f},
        {(right + left) / (left - right), (top + bottom) / (bottom - top), 0.0f, 1.0f},
    };

    Viewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(framebufferWidth),
        .height = static_cast<float>(framebufferHeight),
        .min_depth = 0.0f,
        .max_depth = 1.0f,
    };

    commands.setPipeline(m_pipeline);
    commands.setViewport(0, &viewport, 1);
    commands.pushConstants(shaderStageFlag(ShaderStage::Vertex), 0, sizeof(projection), projection);
    commands.setVertexBuffer(0, m_vertex_buffer);
    commands.setIndexBuffer(m_index_buffer, imguiIndexFormat());

    const ImVec2 clipOffset = draw_data->DisplayPos;
    const ImVec2 clipScale = draw_data->FramebufferScale;
    uint32_t globalIndexOffset = 0;
    int32_t globalVertexOffset = 0;

    for (int listIndex = 0; listIndex < draw_data->CmdListsCount; ++listIndex) {
        const ImDrawList* commandList = draw_data->CmdLists[listIndex];
        for (int commandIndex = 0; commandIndex < commandList->CmdBuffer.Size; ++commandIndex) {
            const ImDrawCmd* drawCommand = &commandList->CmdBuffer[commandIndex];
            if (drawCommand->UserCallback != nullptr) {
                drawCommand->UserCallback(commandList, drawCommand);
                continue;
            }

            const float clipMinX = std::max((drawCommand->ClipRect.x - clipOffset.x) * clipScale.x, 0.0f);
            const float clipMinY = std::max((drawCommand->ClipRect.y - clipOffset.y) * clipScale.y, 0.0f);
            const float clipMaxX =
                std::min((drawCommand->ClipRect.z - clipOffset.x) * clipScale.x, static_cast<float>(framebufferWidth));
            const float clipMaxY =
                std::min((drawCommand->ClipRect.w - clipOffset.y) * clipScale.y, static_cast<float>(framebufferHeight));

            if (clipMaxX <= clipMinX || clipMaxY <= clipMinY) {
                continue;
            }

            ScissorRect scissor{
                .x = static_cast<int32_t>(clipMinX),
                .y = static_cast<int32_t>(static_cast<float>(framebufferHeight) - clipMaxY),
                .width = static_cast<uint32_t>(clipMaxX - clipMinX),
                .height = static_cast<uint32_t>(clipMaxY - clipMinY),
            };

            const BindGroupHandle bindGroup = bindGroupFromTextureId(drawCommand->GetTexID());
            if (bindGroup == 0) {
                continue;
            }

            commands.setScissor(0, &scissor, 1);
            commands.setBindGroup(0, bindGroup);
            commands.drawIndexed(
                drawCommand->ElemCount,
                drawCommand->IdxOffset + globalIndexOffset,
                static_cast<int32_t>(drawCommand->VtxOffset) + globalVertexOffset);
        }

        globalIndexOffset += static_cast<uint32_t>(commandList->IdxBuffer.Size);
        globalVertexOffset += commandList->VtxBuffer.Size;
    }
}

bool TinyRHIImGuiRenderer::createStaticResources()
{
    m_vertex_shader = m_device->createShader(ShaderDesc{.stage = ShaderStage::Vertex, .source = kVertexShader});
    m_fragment_shader = m_device->createShader(ShaderDesc{.stage = ShaderStage::Fragment, .source = kFragmentShader});
    m_sampler = m_device->createSampler(SamplerDesc{
        .min_filter = FilterMode::Linear,
        .mag_filter = FilterMode::Linear,
        .address_u = AddressMode::ClampToEdge,
        .address_v = AddressMode::ClampToEdge,
    });

    BindGroupLayoutDesc bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .count = 1,
    });
    m_bind_group_layout = m_device->createBindGroupLayout(bindGroupLayoutDesc);

    PipelineLayoutDesc pipelineLayoutDesc{};
    pipelineLayoutDesc.bind_group_layouts.push_back(m_bind_group_layout);
    pipelineLayoutDesc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Vertex),
        .offset = 0,
        .size = kProjectionMatrixSize,
    });
    m_pipeline_layout = m_device->createPipelineLayout(pipelineLayoutDesc);

    PipelineDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Triangle;
    pipelineDesc.vertex_input.buffers.push_back(VertexBufferLayoutDesc{
        .binding = 0,
        .stride = sizeof(ImDrawVert),
        .step_mode = VertexStepMode::Vertex,
        .attributes =
            {
                VertexAttributeDesc{
                    .location = 0,
                    .format = VertexFormat::Float2,
                    .offset = offsetof(ImDrawVert, pos),
                },
                VertexAttributeDesc{
                    .location = 1,
                    .format = VertexFormat::Float2,
                    .offset = offsetof(ImDrawVert, uv),
                },
                VertexAttributeDesc{
                    .location = 2,
                    .format = VertexFormat::RGBA8Unorm,
                    .offset = offsetof(ImDrawVert, col),
                },
            },
    });
    pipelineDesc.layout = m_pipeline_layout;
    pipelineDesc.vertex_shader = m_vertex_shader;
    pipelineDesc.fragment_shader = m_fragment_shader;
    ColorTargetState colorTarget{.format = TextureFormat::RGBA8};
    colorTarget.blend.enabled = true;
    pipelineDesc.render_target_state.color_targets.push_back(colorTarget);
    pipelineDesc.depth_state.enabled = false;
    pipelineDesc.raster_state.cull_mode = CullMode::None;
    m_pipeline = m_device->createPipeline(pipelineDesc);

    return m_vertex_shader != 0 && m_fragment_shader != 0 && m_sampler != 0 && m_bind_group_layout != 0
           && m_pipeline_layout != 0 && m_pipeline != 0;
}

bool TinyRHIImGuiRenderer::createFontTexture()
{
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    int bytesPerPixel = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytesPerPixel);
    if (pixels == nullptr || width <= 0 || height <= 0 || bytesPerPixel != 4) {
        return false;
    }

    m_font_texture = m_device->createTexture(TextureDesc{
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .format = TextureFormat::RGBA32,
        .usage = TextureUsage::Sampled | TextureUsage::CopyDst,
    });
    m_font_texture_view = m_device->createTextureView(TextureViewDesc{
        .texture = m_font_texture,
        .format = TextureFormat::RGBA32,
        .aspect = TextureAspect::Color,
    });

    BindGroupDesc bindGroupDesc{};
    bindGroupDesc.layout = m_bind_group_layout;
    bindGroupDesc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .texture_view = m_font_texture_view,
        .sampler = m_sampler,
    });
    m_font_bind_group = m_device->createBindGroup(bindGroupDesc);

    if (m_font_texture == 0 || m_font_texture_view == 0 || m_font_bind_group == 0) {
        return false;
    }

    TextureUploadDesc upload{};
    upload.width = static_cast<uint32_t>(width);
    upload.height = static_cast<uint32_t>(height);
    upload.format = TextureFormat::RGBA32;
    upload.data = pixels;
    upload.row_pitch = static_cast<size_t>(width) * static_cast<size_t>(bytesPerPixel);
    m_device->updateTexture(m_font_texture, upload);

    ImGui::GetIO().Fonts->SetTexID(textureIdFromBindGroup(m_font_bind_group));
    return true;
}

bool TinyRHIImGuiRenderer::ensureBuffers(int vertex_count, int index_count)
{
    const size_t requiredVertexBytes = static_cast<size_t>(vertex_count) * sizeof(ImDrawVert);
    const size_t requiredIndexBytes = static_cast<size_t>(index_count) * sizeof(ImDrawIdx);

    if (m_vertex_buffer == 0 || m_vertex_buffer_size < requiredVertexBytes) {
        if (m_vertex_buffer != 0) {
            m_device->destroyBuffer(m_vertex_buffer);
        }
        m_vertex_buffer_size = std::max(requiredVertexBytes, kInitialVertexBufferSize);
        m_vertex_buffer = m_device->createBuffer(BufferDesc{
            .size = m_vertex_buffer_size,
            .usage = BufferUsage::Vertex | BufferUsage::CopyDst,
            .memory = MemoryUsage::CpuToGpu,
        }, nullptr);
    }

    if (m_index_buffer == 0 || m_index_buffer_size < requiredIndexBytes) {
        if (m_index_buffer != 0) {
            m_device->destroyBuffer(m_index_buffer);
        }
        m_index_buffer_size = std::max(requiredIndexBytes, kInitialIndexBufferSize);
        m_index_buffer = m_device->createBuffer(BufferDesc{
            .size = m_index_buffer_size,
            .usage = BufferUsage::Index | BufferUsage::CopyDst,
            .memory = MemoryUsage::CpuToGpu,
        }, nullptr);
    }

    return m_vertex_buffer != 0 && m_index_buffer != 0;
}

void TinyRHIImGuiRenderer::destroyBuffers()
{
    if (m_vertex_buffer != 0) {
        m_device->destroyBuffer(m_vertex_buffer);
        m_vertex_buffer = 0;
    }
    if (m_index_buffer != 0) {
        m_device->destroyBuffer(m_index_buffer);
        m_index_buffer = 0;
    }
    m_vertex_buffer_size = 0;
    m_index_buffer_size = 0;
}

void TinyRHIImGuiRenderer::destroyFontTexture()
{
    if (m_font_bind_group != 0) {
        m_device->destroyBindGroup(m_font_bind_group);
        m_font_bind_group = 0;
    }
    if (m_font_texture_view != 0) {
        m_device->destroyTextureView(m_font_texture_view);
        m_font_texture_view = 0;
    }
    if (m_font_texture != 0) {
        m_device->destroyTexture(m_font_texture);
        m_font_texture = 0;
    }
}

BindGroupHandle TinyRHIImGuiRenderer::bindGroupFromTextureId(ImTextureID texture_id) const
{
    if (texture_id == ImTextureID_Invalid || texture_id > std::numeric_limits<uint32_t>::max()) {
        return 0;
    }

    return static_cast<BindGroupHandle>(texture_id);
}

} // namespace tinyrhi_examples
