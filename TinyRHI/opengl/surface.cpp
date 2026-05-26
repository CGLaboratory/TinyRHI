#include "surface.h"

namespace lunalite::rhi {

OpenGLSurface::OpenGLSurface(const NativeWindowHandle& native)
    : m_native(native)
{}

NativeWindowHandle OpenGLSurface::getNativeHandle() const
{
    return m_native;
}

uint32_t OpenGLSurface::getWidth() const
{
    return m_width;
}

uint32_t OpenGLSurface::getHeight() const
{
    return m_height;
}

void OpenGLSurface::resize(uint32_t width, uint32_t height)
{
    m_width = width == 0 ? 1 : width;
    m_height = height == 0 ? 1 : height;
}

} // namespace lunalite::rhi
