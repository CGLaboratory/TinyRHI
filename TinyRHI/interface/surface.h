#pragma once
#include "rhi_types.h"

#include <cstdint>

namespace lunalite::rhi {

enum class SurfaceKind {
    OpenGLContext,
    NativeWindow
};

struct OpenGLSurfaceCallbacks {
    void* user_data{nullptr};

    bool (*make_current)(void* user_data){nullptr};
    void* (*get_proc_address)(void* user_data, const char* name){nullptr};
    void (*swap_buffers)(void* user_data){nullptr};
    void (*set_swap_interval)(void* user_data, int interval){nullptr};
    void (*get_framebuffer_size)(void* user_data, uint32_t& width, uint32_t& height){nullptr};
};

struct NativeSurfaceHandle {
    enum class Platform {
        Unknown,
        Win32,
        X11,
        Wayland,
        Cocoa,
        Android
    };

    Platform platform{Platform::Unknown};
    void* display{nullptr};
    void* window{nullptr};
};

struct SurfaceDesc {
    BackendType backend{BackendType::OpenGL};
    SurfaceKind kind{SurfaceKind::OpenGLContext};
    OpenGLSurfaceCallbacks opengl{};
    NativeSurfaceHandle native{};
};

class Surface {
public:
    virtual ~Surface() = default;

    virtual const SurfaceDesc& getSurfaceDesc() const = 0;
    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
};

} // namespace lunalite::rhi
