#include "device.h"
#include "gl_convert.h"

namespace lunalite::rhi {

BufferHandle OpenGLDevice::createBuffer(const BufferDesc& desc, const void* data)
{
    GLuint buffer = 0;
    glCreateBuffers(1, &buffer);
    glNamedBufferData(buffer, static_cast<GLsizeiptr>(desc.size), data, toGLBufferUsage(desc.usage));

    m_buffers.push_back(OpenGLBuffer{.id = buffer, .type = desc.type, .size = desc.size});
    return static_cast<BufferHandle>(m_buffers.size());
}

void OpenGLDevice::updateBuffer(BufferHandle buffer, const void* data, size_t size)
{
    auto* glBuffer = getBuffer(buffer);
    if (glBuffer == nullptr || data == nullptr || size > glBuffer->size) {
        return;
    }

    glNamedBufferSubData(glBuffer->id, 0, static_cast<GLsizeiptr>(size), data);
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
