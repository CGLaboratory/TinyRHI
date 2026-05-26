#include "device.h"
#include "instance.h"
#include "surface.h"

namespace lunalite::rhi {

OpenGLInstance::~OpenGLInstance()
{
    shutdown();
}

bool OpenGLInstance::init()
{
    m_device = std::make_unique<OpenGLDevice>([this](SurfaceHandle surface) {
        return getSurface(surface);
    });
    return true;
}

void OpenGLInstance::shutdown()
{
    m_surfaces.clear();
    m_device.reset();
}

SurfaceHandle OpenGLInstance::createSurface(const NativeWindowHandle& native_window)
{
    if (native_window.platform == NativeWindowHandle::Platform::Unknown || native_window.window == nullptr) {
        return 0;
    }

    auto surface = std::make_unique<OpenGLSurface>(native_window);
    SurfaceHandle handle = static_cast<SurfaceHandle>(m_surfaces.size() + 1);
    m_surfaces.push_back(std::move(surface));
    return handle;
}

void OpenGLInstance::destroySurface(SurfaceHandle surface)
{
    auto* glSurface = getOpenGLSurface(surface);
    if (glSurface == nullptr) {
        return;
    }

    m_surfaces[surface - 1].reset();
}

Surface* OpenGLInstance::getSurface(SurfaceHandle surface)
{
    return getOpenGLSurface(surface);
}

Device* OpenGLInstance::getDevice()
{
    return m_device.get();
}

Surface* OpenGLInstance::getOpenGLSurface(SurfaceHandle handle)
{
    if (handle == 0 || handle > m_surfaces.size()) {
        return nullptr;
    }

    return m_surfaces[handle - 1].get();
}

} // namespace lunalite::rhi
