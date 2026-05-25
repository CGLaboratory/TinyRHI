#include "device.h"
#include "gl_convert.h"

namespace lunalite::rhi {

BufferHandle OpenGLDevice::createBuffer(const BufferDesc& desc, const void* data)
{
    GLuint buffer = 0;
    glCreateBuffers(1, &buffer);
    glNamedBufferData(buffer, static_cast<GLsizeiptr>(desc.size), data, toGLBufferUsage(desc.memory));

    m_buffers.push_back(OpenGLBuffer{.id = buffer, .usage = desc.usage, .memory = desc.memory, .size = desc.size});
    return static_cast<BufferHandle>(m_buffers.size());
}

void OpenGLDevice::updateBuffer(BufferHandle buffer, size_t offset, const void* data, size_t size)
{
    auto* glBuffer = getBuffer(buffer);
    if (glBuffer == nullptr || data == nullptr || offset > glBuffer->size || size > glBuffer->size - offset) {
        return;
    }

    glNamedBufferSubData(glBuffer->id, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
}

void OpenGLDevice::destroyBuffer(BufferHandle buffer)
{
    auto* glBuffer = getBuffer(buffer);
    if (glBuffer == nullptr) {
        return;
    }

    glDeleteBuffers(1, &glBuffer->id);
    glBuffer->id = 0;
    glBuffer->size = 0;
}

} // namespace lunalite::rhi
