#include "common/win32_gl_surface.h"

#include <cstdio>

#include <GL/gl.h>

namespace tinyrhi_examples {

namespace {

using WglCreateContextAttribsARB = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
using WglSwapIntervalEXT = BOOL(WINAPI*)(int);

constexpr const char* kWindowClassName = "TinyRHIExampleWindow";

constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
constexpr int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
constexpr int WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001;
constexpr int WGL_CONTEXT_FLAGS_ARB = 0x2094;
constexpr int WGL_CONTEXT_DEBUG_BIT_ARB = 0x00000001;

WglSwapIntervalEXT g_wglSwapIntervalEXT = nullptr;

void printLastWin32Error(const char* message)
{
    std::printf("%s Win32 error: %lu\n", message, GetLastError());
}

int pixelFormatFlags()
{
    return PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
}

PIXELFORMATDESCRIPTOR pixelFormatDesc()
{
    PIXELFORMATDESCRIPTOR desc{};
    desc.nSize = sizeof(desc);
    desc.nVersion = 1;
    desc.dwFlags = pixelFormatFlags();
    desc.iPixelType = PFD_TYPE_RGBA;
    desc.cColorBits = 32;
    desc.cDepthBits = 24;
    desc.cStencilBits = 8;
    desc.iLayerType = PFD_MAIN_PLANE;
    return desc;
}

bool setWindowPixelFormat(HDC dc)
{
    auto pfd = pixelFormatDesc();
    const int pixelFormat = ChoosePixelFormat(dc, &pfd);
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

void centerWindow(HWND hwnd)
{
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return;
    }

    const int windowWidth = rect.right - rect.left;
    const int windowHeight = rect.bottom - rect.top;
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(
        hwnd,
        nullptr,
        (screenWidth - windowWidth) / 2,
        (screenHeight - windowHeight) / 2,
        0,
        0,
        SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

} // namespace

Win32GLSurface::~Win32GLSurface()
{
    destroy();
}

bool Win32GLSurface::create(
    const char* title,
    uint32_t width,
    uint32_t height,
    const lunalite::rhi::WindowRequirements& requirements)
{
    if (!createWindow(title, width, height)) {
        destroy();
        return false;
    }

    if (!createContext(requirements)) {
        destroy();
        return false;
    }

    m_desc.backend = lunalite::rhi::BackendType::OpenGL;
    m_desc.kind = lunalite::rhi::SurfaceKind::OpenGLContext;
    m_desc.opengl.user_data = this;
    m_desc.opengl.make_current = &Win32GLSurface::makeCurrent;
    m_desc.opengl.get_proc_address = &Win32GLSurface::getProcAddress;
    m_desc.opengl.swap_buffers = &Win32GLSurface::swapBuffers;
    m_desc.opengl.set_swap_interval = &Win32GLSurface::setSwapInterval;
    m_desc.opengl.get_framebuffer_size = &Win32GLSurface::getFramebufferSize;

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

void Win32GLSurface::destroy()
{
    if (m_context != nullptr) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_context);
        m_context = nullptr;
    }

    if (m_hwnd != nullptr && m_dc != nullptr) {
        ReleaseDC(m_hwnd, m_dc);
        m_dc = nullptr;
    }

    if (m_hwnd != nullptr) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    m_width = 0;
    m_height = 0;
    m_should_close = false;
    m_desc = {};
}

bool Win32GLSurface::pollEvents()
{
    MSG msg{};
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_should_close = true;
        }

        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return !m_should_close;
}

bool Win32GLSurface::shouldClose() const
{
    return m_should_close;
}

void Win32GLSurface::requestClose()
{
    m_should_close = true;
}

const lunalite::rhi::SurfaceDesc& Win32GLSurface::getSurfaceDesc() const
{
    return m_desc;
}

uint32_t Win32GLSurface::getWidth() const
{
    return m_width;
}

uint32_t Win32GLSurface::getHeight() const
{
    return m_height;
}

void Win32GLSurface::resize(uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;
}

HWND Win32GLSurface::hwnd() const
{
    return m_hwnd;
}

LRESULT CALLBACK Win32GLSurface::windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* surface = reinterpret_cast<Win32GLSurface*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    switch (message) {
        case WM_NCCREATE: {
            const auto* create = reinterpret_cast<CREATESTRUCTA*>(lparam);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }
        case WM_CLOSE:
            if (surface != nullptr) {
                surface->requestClose();
            }
            return 0;
        case WM_SIZE:
            if (surface != nullptr) {
                const auto width = static_cast<uint32_t>(LOWORD(lparam));
                const auto height = static_cast<uint32_t>(HIWORD(lparam));
                surface->handleResize(width, height);
            }
            return 0;
        default:
            return DefWindowProcA(hwnd, message, wparam, lparam);
    }
}

bool Win32GLSurface::makeCurrent(void* userData)
{
    auto* surface = static_cast<Win32GLSurface*>(userData);
    return surface != nullptr && wglMakeCurrent(surface->m_dc, surface->m_context) == TRUE;
}

void* Win32GLSurface::getProcAddress(void* userData, const char* name)
{
    static_cast<void>(userData);

    void* proc = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (proc != nullptr) {
        return proc;
    }

    HMODULE opengl = GetModuleHandleA("opengl32.dll");
    if (opengl == nullptr) {
        opengl = LoadLibraryA("opengl32.dll");
    }

    return opengl != nullptr ? reinterpret_cast<void*>(GetProcAddress(opengl, name)) : nullptr;
}

void Win32GLSurface::swapBuffers(void* userData)
{
    auto* surface = static_cast<Win32GLSurface*>(userData);
    if (surface != nullptr && surface->m_dc != nullptr) {
        SwapBuffers(surface->m_dc);
    }
}

void Win32GLSurface::setSwapInterval(void* userData, int interval)
{
    static_cast<void>(userData);
    if (g_wglSwapIntervalEXT != nullptr) {
        g_wglSwapIntervalEXT(interval);
    }
}

void Win32GLSurface::getFramebufferSize(void* userData, uint32_t& width, uint32_t& height)
{
    const auto* surface = static_cast<const Win32GLSurface*>(userData);
    if (surface == nullptr || surface->m_hwnd == nullptr) {
        return;
    }

    RECT client{};
    if (GetClientRect(surface->m_hwnd, &client)) {
        width = static_cast<uint32_t>(client.right - client.left);
        height = static_cast<uint32_t>(client.bottom - client.top);
    }
}

bool Win32GLSurface::createWindow(const char* title, uint32_t width, uint32_t height)
{
    m_instance = GetModuleHandleA(nullptr);

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = &Win32GLSurface::windowProc;
    wc.hInstance = m_instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;

    if (!RegisterClassExA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        printLastWin32Error("RegisterClassExA failed.");
        return false;
    }

    RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindowExA(
        0,
        kWindowClassName,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        m_instance,
        this);
    if (m_hwnd == nullptr) {
        printLastWin32Error("CreateWindowExA failed.");
        return false;
    }

    centerWindow(m_hwnd);

    m_dc = GetDC(m_hwnd);
    if (m_dc == nullptr) {
        printLastWin32Error("GetDC failed.");
        return false;
    }

    m_width = width;
    m_height = height;
    return true;
}

bool Win32GLSurface::createContext(const lunalite::rhi::WindowRequirements& requirements)
{
    if (!setWindowPixelFormat(m_dc)) {
        return false;
    }

    HGLRC legacyContext = wglCreateContext(m_dc);
    if (legacyContext == nullptr) {
        printLastWin32Error("wglCreateContext failed.");
        return false;
    }

    if (!wglMakeCurrent(m_dc, legacyContext)) {
        printLastWin32Error("wglMakeCurrent failed.");
        wglDeleteContext(legacyContext);
        return false;
    }

    auto createContextAttribs =
        reinterpret_cast<WglCreateContextAttribsARB>(wglGetProcAddress("wglCreateContextAttribsARB"));
    g_wglSwapIntervalEXT = reinterpret_cast<WglSwapIntervalEXT>(wglGetProcAddress("wglSwapIntervalEXT"));

    if (createContextAttribs == nullptr) {
        std::printf("wglCreateContextAttribsARB is unavailable; OpenGL %d.%d context creation failed.\n",
                    requirements.glMajor,
                    requirements.glMinor);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(legacyContext);
        return false;
    }

    const int profile = requirements.gl_core_profile ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB : 0;
    const auto tryCreateContext = [&](int flags) {
        const int attributes[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB,
            requirements.glMajor,
            WGL_CONTEXT_MINOR_VERSION_ARB,
            requirements.glMinor,
            WGL_CONTEXT_PROFILE_MASK_ARB,
            profile,
            WGL_CONTEXT_FLAGS_ARB,
            flags,
            0,
        };

        return createContextAttribs(m_dc, nullptr, attributes);
    };

    HGLRC modernContext = tryCreateContext(requirements.gl_debug_context ? WGL_CONTEXT_DEBUG_BIT_ARB : 0);
    if (modernContext == nullptr && requirements.gl_debug_context) {
        modernContext = tryCreateContext(0);
    }

    if (modernContext != nullptr) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(legacyContext);
        m_context = modernContext;
        return wglMakeCurrent(m_dc, m_context) == TRUE;
    }

    if (!requirements.gl_core_profile) {
        m_context = legacyContext;
        return true;
    }

    std::printf("Failed to create OpenGL %d.%d context.\n", requirements.glMajor, requirements.glMinor);
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(legacyContext);
    return false;
}

void Win32GLSurface::handleResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        return;
    }

    m_width = width;
    m_height = height;
}

} // namespace tinyrhi_examples
