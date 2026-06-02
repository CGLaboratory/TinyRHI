#include "native_swapchain.h"

#include <cstdio>
#include <cstdint>

#include <glad/glad.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace lunalite::rhi {

namespace {

#if defined(_WIN32)
using WglCreateContextAttribsARB = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
using WglChoosePixelFormatARB = BOOL(WINAPI*)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
using WglSwapIntervalEXT = BOOL(WINAPI*)(int);

constexpr int kOpenGLMajorVersion = 4;
constexpr int kOpenGLMinorVersion = 5;
constexpr bool kOpenGLDebugContext = true;

constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
constexpr int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
constexpr int WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001;
constexpr int WGL_CONTEXT_FLAGS_ARB = 0x2094;
constexpr int WGL_CONTEXT_DEBUG_BIT_ARB = 0x00000001;
constexpr int WGL_DRAW_TO_WINDOW_ARB = 0x2001;
constexpr int WGL_SUPPORT_OPENGL_ARB = 0x2010;
constexpr int WGL_DOUBLE_BUFFER_ARB = 0x2011;
constexpr int WGL_PIXEL_TYPE_ARB = 0x2013;
constexpr int WGL_COLOR_BITS_ARB = 0x2014;
constexpr int WGL_DEPTH_BITS_ARB = 0x2022;
constexpr int WGL_STENCIL_BITS_ARB = 0x2023;
constexpr int WGL_TYPE_RGBA_ARB = 0x202B;
constexpr int WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB = 0x20A9;

WglCreateContextAttribsARB g_wglCreateContextAttribsARB = nullptr;
WglChoosePixelFormatARB g_wglChoosePixelFormatARB = nullptr;
WglSwapIntervalEXT g_wglSwapIntervalEXT = nullptr;

void printLastWin32Error(const char* message)
{
    std::printf("%s Win32 error: %lu\n", message, GetLastError());
}

void* getOpenGLProcAddress(const char* name)
{
    void* proc = reinterpret_cast<void*>(wglGetProcAddress(name));
    const auto procValue = reinterpret_cast<uintptr_t>(proc);
    if (procValue > 3 && procValue != static_cast<uintptr_t>(-1)) {
        return proc;
    }

    HMODULE opengl = GetModuleHandleA("opengl32.dll");
    if (opengl == nullptr) {
        opengl = LoadLibraryA("opengl32.dll");
    }

    return opengl != nullptr ? reinterpret_cast<void*>(GetProcAddress(opengl, name)) : nullptr;
}

PIXELFORMATDESCRIPTOR pixelFormatDesc()
{
    PIXELFORMATDESCRIPTOR desc{};
    desc.nSize = sizeof(desc);
    desc.nVersion = 1;
    desc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    desc.iPixelType = PFD_TYPE_RGBA;
    desc.cColorBits = 32;
    desc.cDepthBits = 24;
    desc.cStencilBits = 8;
    desc.iLayerType = PFD_MAIN_PLANE;
    return desc;
}

bool loadWglPixelFormatExtensions()
{
    if (g_wglChoosePixelFormatARB != nullptr) {
        return true;
    }

    HINSTANCE instance = GetModuleHandleA(nullptr);
    constexpr const char* kDummyClassName = "TinyRHIWGLDummyWindow";
    WNDCLASSA windowClass{};
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = DefWindowProcA;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kDummyClassName;
    if (!RegisterClassA(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        printLastWin32Error("RegisterClassA for WGL dummy window failed.");
        return false;
    }

    HWND window = CreateWindowA(kDummyClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    if (window == nullptr) {
        printLastWin32Error("CreateWindowA for WGL dummy window failed.");
        return false;
    }

    HDC dc = GetDC(window);
    bool loaded = false;
    if (dc != nullptr) {
        auto pfd = pixelFormatDesc();
        const int pixelFormat = ChoosePixelFormat(dc, &pfd);
        if (pixelFormat != 0 && SetPixelFormat(dc, pixelFormat, &pfd)) {
            HGLRC context = wglCreateContext(dc);
            if (context != nullptr && wglMakeCurrent(dc, context)) {
                g_wglChoosePixelFormatARB =
                    reinterpret_cast<WglChoosePixelFormatARB>(wglGetProcAddress("wglChoosePixelFormatARB"));
                g_wglCreateContextAttribsARB =
                    reinterpret_cast<WglCreateContextAttribsARB>(wglGetProcAddress("wglCreateContextAttribsARB"));
                g_wglSwapIntervalEXT = reinterpret_cast<WglSwapIntervalEXT>(wglGetProcAddress("wglSwapIntervalEXT"));
                loaded = g_wglChoosePixelFormatARB != nullptr;
                wglMakeCurrent(nullptr, nullptr);
            }

            if (context != nullptr) {
                wglDeleteContext(context);
            }
        }

        ReleaseDC(window, dc);
    }

    DestroyWindow(window);
    return loaded;
}

bool chooseSrgbPixelFormat(HDC dc, int& pixelFormat)
{
    if (!loadWglPixelFormatExtensions()) {
        return false;
    }

    const int attributes[] = {
        WGL_DRAW_TO_WINDOW_ARB,
        GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB,
        GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB,
        GL_TRUE,
        WGL_PIXEL_TYPE_ARB,
        WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB,
        32,
        WGL_DEPTH_BITS_ARB,
        24,
        WGL_STENCIL_BITS_ARB,
        8,
        WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB,
        GL_TRUE,
        0,
    };

    UINT formatCount = 0;
    pixelFormat = 0;
    return g_wglChoosePixelFormatARB(dc, attributes, nullptr, 1, &pixelFormat, &formatCount) && formatCount > 0 &&
           pixelFormat != 0;
}

bool setWindowPixelFormat(HDC dc, bool preferSrgb, bool& framebufferSrgbCapable)
{
    if (GetPixelFormat(dc) != 0) {
        return true;
    }

    auto pfd = pixelFormatDesc();
    int pixelFormat = 0;
    framebufferSrgbCapable = preferSrgb && chooseSrgbPixelFormat(dc, pixelFormat);
    if (pixelFormat != 0) {
        DescribePixelFormat(dc, pixelFormat, sizeof(pfd), &pfd);
    } else {
        pixelFormat = ChoosePixelFormat(dc, &pfd);
    }

    if (pixelFormat == 0) {
        printLastWin32Error("ChoosePixelFormat failed.");
        return false;
    }

    if (!SetPixelFormat(dc, pixelFormat, &pfd)) {
        printLastWin32Error("SetPixelFormat failed.");
        return false;
    }

    return true;
}

HGLRC createModernContext(HDC dc)
{
    HGLRC legacyContext = wglCreateContext(dc);
    if (legacyContext == nullptr) {
        printLastWin32Error("wglCreateContext failed.");
        return nullptr;
    }

    if (!wglMakeCurrent(dc, legacyContext)) {
        printLastWin32Error("wglMakeCurrent failed.");
        wglDeleteContext(legacyContext);
        return nullptr;
    }

    if (g_wglCreateContextAttribsARB == nullptr) {
        g_wglCreateContextAttribsARB =
            reinterpret_cast<WglCreateContextAttribsARB>(wglGetProcAddress("wglCreateContextAttribsARB"));
    }
    if (g_wglSwapIntervalEXT == nullptr) {
        g_wglSwapIntervalEXT = reinterpret_cast<WglSwapIntervalEXT>(wglGetProcAddress("wglSwapIntervalEXT"));
    }

    if (g_wglCreateContextAttribsARB == nullptr) {
        std::printf("wglCreateContextAttribsARB is unavailable; OpenGL %d.%d context creation failed.\n",
                    kOpenGLMajorVersion,
                    kOpenGLMinorVersion);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(legacyContext);
        return nullptr;
    }

    const auto tryCreateContext = [&](int flags) {
        const int attributes[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB,
            kOpenGLMajorVersion,
            WGL_CONTEXT_MINOR_VERSION_ARB,
            kOpenGLMinorVersion,
            WGL_CONTEXT_PROFILE_MASK_ARB,
            WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            WGL_CONTEXT_FLAGS_ARB,
            flags,
            0,
        };

        return g_wglCreateContextAttribsARB(dc, nullptr, attributes);
    };

    HGLRC modernContext = tryCreateContext(kOpenGLDebugContext ? WGL_CONTEXT_DEBUG_BIT_ARB : 0);
    if (modernContext == nullptr && kOpenGLDebugContext) {
        modernContext = tryCreateContext(0);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(legacyContext);

    if (modernContext == nullptr) {
        std::printf("Failed to create OpenGL %d.%d context.\n", kOpenGLMajorVersion, kOpenGLMinorVersion);
    }

    return modernContext;
}
#endif

} // namespace

bool createOpenGLNativeSwapchain(const NativeSurfaceHandle& native, OpenGLNativeSwapchain& swapchain, bool prefer_srgb)
{
#if defined(_WIN32)
    if (native.platform != NativeSurfaceHandle::Platform::Win32 || native.window == nullptr) {
        std::printf("OpenGL native swapchain creation failed: expected a Win32 native surface.\n");
        return false;
    }

    auto hwnd = static_cast<HWND>(native.window);
    auto dc = GetDC(hwnd);
    if (dc == nullptr) {
        printLastWin32Error("GetDC failed.");
        return false;
    }

    bool framebufferSrgbCapable = false;
    if (!setWindowPixelFormat(dc, prefer_srgb, framebufferSrgbCapable)) {
        ReleaseDC(hwnd, dc);
        return false;
    }
    if (prefer_srgb && !framebufferSrgbCapable) {
        std::printf("OpenGL native swapchain creation: sRGB framebuffer pixel format unavailable; falling back.\n");
    }

    swapchain.window = hwnd;
    swapchain.drawable = dc;
    return true;
#else
    static_cast<void>(native);
    static_cast<void>(swapchain);
    static_cast<void>(prefer_srgb);
    std::printf("OpenGL native swapchain creation failed: Win32/WGL is the only implemented OpenGL surface backend.\n");
    return false;
#endif
}

void destroyOpenGLNativeSwapchain(OpenGLNativeContext& context, OpenGLNativeSwapchain& swapchain)
{
#if defined(_WIN32)
    if (context.current_drawable == swapchain.drawable) {
        wglMakeCurrent(nullptr, nullptr);
        context.current_drawable = nullptr;
    }

    if (swapchain.window != nullptr && swapchain.drawable != nullptr) {
        ReleaseDC(static_cast<HWND>(swapchain.window), static_cast<HDC>(swapchain.drawable));
    }
#else
    static_cast<void>(context);
#endif

    swapchain.window = nullptr;
    swapchain.drawable = nullptr;
}

bool ensureOpenGLNativeContext(OpenGLNativeContext& context, OpenGLNativeSwapchain& swapchain, bool vsync)
{
#if defined(_WIN32)
    auto dc = static_cast<HDC>(swapchain.drawable);
    if (dc == nullptr) {
        return false;
    }

    const bool createdContext = context.context == nullptr;
    if (createdContext) {
        HGLRC glContext = createModernContext(dc);
        if (glContext == nullptr) {
            return false;
        }

        context.context = glContext;
    }

    if (!makeOpenGLNativeContextCurrent(context, swapchain)) {
        if (createdContext) {
            destroyOpenGLNativeContext(context);
        }
        return false;
    }

    if (!context.gl_loaded) {
        const int loaded = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(getOpenGLProcAddress));
        if (!loaded) {
            std::printf("gladLoadGLLoader failed.\n");
            if (createdContext) {
                destroyOpenGLNativeContext(context);
            }
            return false;
        }
        context.gl_loaded = true;
    }

    if (g_wglSwapIntervalEXT != nullptr) {
        g_wglSwapIntervalEXT(vsync ? 1 : 0);
    }

    return true;
#else
    static_cast<void>(context);
    static_cast<void>(swapchain);
    static_cast<void>(vsync);
    return false;
#endif
}

bool makeOpenGLNativeContextCurrent(OpenGLNativeContext& context, OpenGLNativeSwapchain& swapchain)
{
#if defined(_WIN32)
    auto dc = static_cast<HDC>(swapchain.drawable);
    auto glContext = static_cast<HGLRC>(context.context);
    if (dc == nullptr || glContext == nullptr) {
        return false;
    }

    if (context.current_drawable == swapchain.drawable) {
        return true;
    }

    if (!wglMakeCurrent(dc, glContext)) {
        printLastWin32Error("wglMakeCurrent failed.");
        return false;
    }

    context.current_drawable = swapchain.drawable;
    return true;
#else
    static_cast<void>(context);
    static_cast<void>(swapchain);
    return false;
#endif
}

void destroyOpenGLNativeContext(OpenGLNativeContext& context)
{
#if defined(_WIN32)
    if (context.context != nullptr) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(static_cast<HGLRC>(context.context));
    }
#endif

    context.context = nullptr;
    context.current_drawable = nullptr;
    context.gl_loaded = false;
}

void presentOpenGLNativeSwapchain(OpenGLNativeSwapchain& swapchain)
{
#if defined(_WIN32)
    if (swapchain.drawable != nullptr) {
        SwapBuffers(static_cast<HDC>(swapchain.drawable));
    }
#else
    static_cast<void>(swapchain);
#endif
}

} // namespace lunalite::rhi
