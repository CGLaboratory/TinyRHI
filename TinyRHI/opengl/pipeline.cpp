#include "device.h"
#include "gl_convert.h"

#include <cstdio>

#include <vector>

namespace lunalite::rhi {
namespace {
void logProgramError(GLuint program)
{
    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength <= 1) {
        return;
    }

    std::vector<GLchar> log(static_cast<size_t>(logLength));
    glGetProgramInfoLog(program, logLength, nullptr, log.data());
    std::printf("OpenGL program link failed:\n%s\n", log.data());
}
} // namespace

PipelineHandle OpenGLDevice::createPipeline(const PipelineDesc& desc)
{
    if (getPipelineLayout(desc.layout) == nullptr) {
        std::printf("OpenGL pipeline creation failed: invalid pipeline layout handle.\n");
        return {};
    }

    if (desc.render_target_state.color_targets.empty() && !desc.render_target_state.has_depth_stencil) {
        std::printf("OpenGL pipeline creation failed: render target state has no attachments.\n");
        return {};
    }

    const auto* vertexShader = getShader(desc.vertex_shader);
    const auto* fragmentShader = getShader(desc.fragment_shader);
    if (vertexShader == nullptr || fragmentShader == nullptr) {
        std::printf("OpenGL pipeline creation failed: invalid shader handle.\n");
        return {};
    }
    if (vertexShader->stage != ShaderStage::Vertex || fragmentShader->stage != ShaderStage::Fragment) {
        std::printf("OpenGL pipeline creation failed: shader stages must be vertex and fragment.\n");
        return {};
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader->id);
    glAttachShader(program, fragmentShader->id);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        logProgramError(program);
        glDeleteProgram(program);
        return {};
    }

    GLuint vao = 0;
    glCreateVertexArrays(1, &vao);

    for (const auto& buffer : desc.vertex_input.buffers) {
        glVertexArrayBindingDivisor(vao, buffer.binding, buffer.step_mode == VertexStepMode::Instance ? 1 : 0);

        for (const auto& attribute : buffer.attributes) {
            const auto location = static_cast<GLuint>(attribute.location);
            const auto componentCount = static_cast<GLint>(vertexFormatComponentCount(attribute.format));
            const GLenum type = vertexFormatType(attribute.format);

            glEnableVertexArrayAttrib(vao, location);

            if (isIntegerVertexFormat(attribute.format)) {
                glVertexArrayAttribIFormat(vao, location, componentCount, type, attribute.offset);
            } else {
                glVertexArrayAttribFormat(vao,
                                          location,
                                          componentCount,
                                          type,
                                          isNormalizedVertexFormat(attribute.format) ? GL_TRUE : GL_FALSE,
                                          attribute.offset);
            }

            glVertexArrayAttribBinding(vao, location, buffer.binding);
        }
    }

    m_pipelines.push_back(OpenGLPipeline{
        .program = program,
        .vao = vao,
        .type = OpenGLPipelineType::Graphics,
        .topology = toGLTopology(desc.topology),
        .vertex_input = desc.vertex_input,
        .layout = desc.layout,
        .render_target_state = desc.render_target_state,
        .depth_state = desc.depth_state,
        .raster_state = desc.raster_state,
    });
    return makeHandle<PipelineHandle>(m_pipelines.size() - 1);
}

PipelineHandle OpenGLDevice::createComputePipeline(const ComputePipelineDesc& desc)
{
    if (getPipelineLayout(desc.layout) == nullptr) {
        std::printf("OpenGL compute pipeline creation failed: invalid pipeline layout handle.\n");
        return {};
    }

    const auto* computeShader = getShader(desc.compute_shader);
    if (computeShader == nullptr) {
        std::printf("OpenGL compute pipeline creation failed: invalid shader handle.\n");
        return {};
    }
    if (computeShader->stage != ShaderStage::Compute) {
        std::printf("OpenGL compute pipeline creation failed: shader stage must be compute.\n");
        return {};
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, computeShader->id);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        logProgramError(program);
        glDeleteProgram(program);
        return {};
    }

    m_pipelines.push_back(OpenGLPipeline{
        .program = program,
        .vao = 0,
        .type = OpenGLPipelineType::Compute,
        .topology = GL_TRIANGLES,
        .vertex_input = VertexInputDesc{},
        .layout = desc.layout,
        .render_target_state = RenderTargetState{},
        .depth_state = DepthState{},
        .raster_state = RasterState{},
    });
    return makeHandle<PipelineHandle>(m_pipelines.size() - 1);
}

void OpenGLDevice::destroyPipeline(PipelineHandle pipeline)
{
    auto* glPipeline = getPipeline(pipeline);
    if (glPipeline == nullptr) {
        return;
    }

    if (glPipeline->vao != 0) {
        glDeleteVertexArrays(1, &glPipeline->vao);
    }
    glDeleteProgram(glPipeline->program);
    glPipeline->vao = 0;
    glPipeline->program = 0;
}

} // namespace lunalite::rhi
