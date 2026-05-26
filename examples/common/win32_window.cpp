#include "common/win32_window.h"

#include <cstdio>

namespace tinyrhi_examples {

namespace {

constexpr const char* kWindowClassName = "TinyRHIExampleWindow";

void printLastWin32Error(const char* message)
{
    std::printf("%s Win32 error: %lu\n", message, GetLastError());
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

Win32Window::~Win32Window()
{
    destroy();
}

bool Win32Window::create(const char* title, uint32_t width, uint32_t height)
{
    if (!createWindow(title, width, height)) {
        destroy();
        return false;
    }

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

void Win32Window::destroy()
{
    if (m_hwnd != nullptr) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    m_width = 0;
    m_height = 0;
    m_should_close = false;
}

bool Win32Window::pollEvents()
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

bool Win32Window::shouldClose() const
{
    return m_should_close;
}

void Win32Window::requestClose()
{
    m_should_close = true;
}

lunalite::rhi::NativeWindowHandle Win32Window::nativeWindow() const
{
    return lunalite::rhi::NativeWindowHandle{
        .platform = lunalite::rhi::NativeWindowHandle::Platform::Win32,
        .display = nullptr,
        .window = m_hwnd,
    };
}

uint32_t Win32Window::getWidth() const
{
    return m_width;
}

uint32_t Win32Window::getHeight() const
{
    return m_height;
}

HWND Win32Window::hwnd() const
{
    return m_hwnd;
}

LRESULT CALLBACK Win32Window::windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* window = reinterpret_cast<Win32Window*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    switch (message) {
        case WM_NCCREATE: {
            const auto* create = reinterpret_cast<CREATESTRUCTA*>(lparam);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }
        case WM_CLOSE:
            if (window != nullptr) {
                window->requestClose();
            }
            return 0;
        case WM_SIZE:
            if (window != nullptr) {
                const auto width = static_cast<uint32_t>(LOWORD(lparam));
                const auto height = static_cast<uint32_t>(HIWORD(lparam));
                window->handleResize(width, height);
            }
            return 0;
        default:
            return DefWindowProcA(hwnd, message, wparam, lparam);
    }
}

bool Win32Window::createWindow(const char* title, uint32_t width, uint32_t height)
{
    m_instance = GetModuleHandleA(nullptr);

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = &Win32Window::windowProc;
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

    m_width = width;
    m_height = height;
    return true;
}

void Win32Window::handleResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        return;
    }

    m_width = width;
    m_height = height;
}

} // namespace tinyrhi_examples
