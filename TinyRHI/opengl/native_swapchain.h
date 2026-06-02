#pragma once

#include "../interface/surface.h"

namespace lunalite::rhi {

struct OpenGLNativeContext {
    void* context{nullptr};
    void* current_drawable{nullptr};
    bool gl_loaded{false};
};

struct OpenGLNativeSwapchain {
    void* window{nullptr};
    void* drawable{nullptr};
};

bool createOpenGLNativeSwapchain(const NativeSurfaceHandle& native, OpenGLNativeSwapchain& swapchain, bool prefer_srgb);
void destroyOpenGLNativeSwapchain(OpenGLNativeContext& context, OpenGLNativeSwapchain& swapchain);

bool ensureOpenGLNativeContext(OpenGLNativeContext& context, OpenGLNativeSwapchain& swapchain, bool vsync);
bool makeOpenGLNativeContextCurrent(OpenGLNativeContext& context, OpenGLNativeSwapchain& swapchain);
void destroyOpenGLNativeContext(OpenGLNativeContext& context);
void presentOpenGLNativeSwapchain(OpenGLNativeSwapchain& swapchain);

} // namespace lunalite::rhi
