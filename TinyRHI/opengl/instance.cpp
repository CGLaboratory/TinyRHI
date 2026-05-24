#include "device.h"
#include "instance.h"
#include "swapchain.h"

#include <glad/glad.h>

namespace lunalite::rhi {

OpenGLInstance::~OpenGLInstance() = default;

namespace {
const OpenGLSurfaceCallbacks* s_loader_callbacks = nullptr;

void* loadOpenGLProc(const char* name)
{
    if (s_loader_callbacks == nullptr || s_loader_callbacks->get_proc_address == nullptr) {
        return nullptr;
    }

    return s_loader_callbacks->get_proc_address(s_loader_callbacks->user_data, name);
}
} // namespace

bool OpenGLInstance::init(Surface& surface)
{
    const auto& desc = surface.getSurfaceDesc();
    if (desc.backend != BackendType::OpenGL || desc.kind != SurfaceKind::OpenGLContext) {
        return false;
    }

    const auto& gl_surface = desc.opengl;
    if (gl_surface.make_current == nullptr || gl_surface.get_proc_address == nullptr ||
        gl_surface.swap_buffers == nullptr) {
        return false;
    }

    if (!gl_surface.make_current(gl_surface.user_data)) {
        return false;
    }

    s_loader_callbacks = &gl_surface;
    const int loaded = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(loadOpenGLProc));
    s_loader_callbacks = nullptr;
    if (!loaded) {
        return false;
    }

    if (gl_surface.set_swap_interval != nullptr) {
        gl_surface.set_swap_interval(gl_surface.user_data, 1);
    }

    auto device = std::make_unique<OpenGLDevice>();
    m_surface = &surface;
    m_swapchain = std::make_unique<OpenGLSwapchain>(*device, surface);
    m_device = std::move(device);
    return true;
}

void OpenGLInstance::shutdown()
{
    m_swapchain.reset();
    m_device.reset();
    m_surface = nullptr;
}

void OpenGLInstance::resize(uint32_t width, uint32_t height)
{
    glViewport(0, 0, static_cast<int>(width), static_cast<int>(height));
    if (m_swapchain) {
        m_swapchain->resize(width, height);
    }
}

Device* OpenGLInstance::getDevice()
{
    return m_device.get();
}

Swapchain* OpenGLInstance::getSwapchain()
{
    return m_swapchain.get();
}

} // namespace lunalite::rhi
