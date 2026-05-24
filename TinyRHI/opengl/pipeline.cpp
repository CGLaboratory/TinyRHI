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
        return 0;
    }

    if (desc.render_target_state.color_targets.empty() && !desc.render_target_state.has_depth_stencil) {
        std::printf("OpenGL pipeline creation failed: render target state has no attachments.\n");
        return 0;
    }

    const auto* vertexShader = getShader(desc.vertex_shader);
    const auto* fragmentShader = getShader(desc.fragment_shader);
    if (vertexShader == nullptr || fragmentShader == nullptr) {
        std::printf("OpenGL pipeline creation failed: invalid shader handle.\n");
        return 0;
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
        return 0;
    }

    GLuint vao = 0;
    glCreateVertexArrays(1, &vao);

    for (const auto& attribute : desc.vertex_layout.attributes) {
        const GLuint location = vertexAttributeLocation(attribute.semantic);
        const auto componentCount = static_cast<GLint>(vertexFormatComponentCount(attribute.format));
        const GLenum type = vertexFormatType(attribute.format);

        glEnableVertexArrayAttrib(vao, location);

        if (isIntegerVertexFormat(attribute.format)) {
            glVertexArrayAttribIFormat(vao, location, componentCount, type, attribute.offset);
        } else {
            glVertexArrayAttribFormat(vao, location, componentCount, type, GL_FALSE, attribute.offset);
        }

        glVertexArrayAttribBinding(vao, location, 0);
    }

    m_pipelines.push_back(OpenGLPipeline{
        .program = program,
        .vao = vao,
        .topology = toGLTopology(desc.topology),
        .vertex_layout = desc.vertex_layout,
        .layout = desc.layout,
        .render_target_state = desc.render_target_state,
        .depth_state = desc.depth_state,
        .raster_state = desc.raster_state,
    });
    return static_cast<PipelineHandle>(m_pipelines.size());
}

void OpenGLDevice::destroyPipeline(PipelineHandle pipeline)
{
    auto* glPipeline = getPipeline(pipeline);
    if (glPipeline == nullptr) {
        return;
    }

    glDeleteVertexArrays(1, &glPipeline->vao);
    glDeleteProgram(glPipeline->program);
    glPipeline->vao = 0;
    glPipeline->program = 0;
}

} // namespace lunalite::rhi
