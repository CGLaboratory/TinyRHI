#include "device.h"
#include "gl_convert.h"

#include <cstdio>
#include <vector>

namespace lunalite::rhi {
namespace {
void logShaderError(GLuint shader)
{
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength <= 1) {
        return;
    }

    std::vector<GLchar> log(static_cast<size_t>(logLength));
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    std::printf("OpenGL shader compile failed:\n%s\n", log.data());
}
} // namespace

ShaderHandle OpenGLDevice::createShader(const ShaderDesc& desc)
{
    GLuint shader = glCreateShader(toGLShaderStage(desc.stage));
    glShaderSource(shader, 1, &desc.source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        logShaderError(shader);
        glDeleteShader(shader);
        return 0;
    }

    m_shaders.push_back(OpenGLShader{.id = shader, .stage = desc.stage});
    return static_cast<ShaderHandle>(m_shaders.size());
}

void OpenGLDevice::destroyShader(ShaderHandle shader)
{
    auto* glShader = getShader(shader);
    if (glShader == nullptr) {
        return;
    }

    glDeleteShader(glShader->id);
    glShader->id = 0;
}

} // namespace lunalite::rhi
