#include "device.h"
#include "gl_convert.h"

namespace lunalite::rhi {
namespace {
bool hasUsage(BufferUsage usage, BufferUsage required)
{
    return (usage & required) == required;
}

bool bufferInitialStateCompatible(BufferUsage usage, ResourceState state)
{
    switch (state) {
        case ResourceState::Undefined:
            return true;
        case ResourceState::CopySrc:
            return hasUsage(usage, BufferUsage::CopySrc);
        case ResourceState::CopyDst:
            return hasUsage(usage, BufferUsage::CopyDst);
        case ResourceState::VertexBuffer:
            return hasUsage(usage, BufferUsage::Vertex);
        case ResourceState::IndexBuffer:
            return hasUsage(usage, BufferUsage::Index);
        case ResourceState::IndirectArgument:
            return hasUsage(usage, BufferUsage::Indirect);
        case ResourceState::UniformRead:
            return hasUsage(usage, BufferUsage::Uniform);
        case ResourceState::ShaderRead:
        case ResourceState::StorageRead:
        case ResourceState::StorageReadWrite:
            return hasUsage(usage, BufferUsage::Storage);
        case ResourceState::ColorAttachment:
        case ResourceState::DepthStencilRead:
        case ResourceState::DepthStencilWrite:
        case ResourceState::Present:
            return false;
    }

    return false;
}
} // namespace

BufferHandle OpenGLDevice::createBuffer(const BufferDesc& desc, const void* data)
{
    if (!bufferInitialStateCompatible(desc.usage, desc.initial_state)) {
        return {};
    }

    GLuint buffer = 0;
    glCreateBuffers(1, &buffer);
    glNamedBufferData(buffer, static_cast<GLsizeiptr>(desc.size), data, toGLBufferUsage(desc.memory));

    m_buffers.push_back(OpenGLBuffer{
        .id = buffer,
        .usage = desc.usage,
        .memory = desc.memory,
        .size = desc.size,
        .state = desc.initial_state,
    });
    return makeHandle<BufferHandle>(m_buffers.size() - 1);
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
