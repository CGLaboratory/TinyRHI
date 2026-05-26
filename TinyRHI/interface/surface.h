#pragma once
#include "rhi_types.h"

#include <cstdint>

namespace lunalite::rhi {

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

class Surface {
public:
    virtual ~Surface() = default;

    virtual NativeSurfaceHandle getNativeHandle() const = 0;
    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
};

} // namespace lunalite::rhi
