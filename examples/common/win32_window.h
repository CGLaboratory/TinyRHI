#pragma once

#include "TinyRHI/interface/surface.h"

#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace tinyrhi_examples {

class Win32Window final {
public:
    Win32Window() = default;
    ~Win32Window();

    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    bool create(const char* title, uint32_t width, uint32_t height);
    void destroy();

    bool pollEvents();
    bool shouldClose() const;
    void requestClose();

    lunalite::rhi::NativeWindowHandle nativeWindow() const;
    uint32_t getWidth() const;
    uint32_t getHeight() const;

    HWND hwnd() const;

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    bool createWindow(const char* title, uint32_t width, uint32_t height);
    void handleResize(uint32_t width, uint32_t height);

    HINSTANCE m_instance{nullptr};
    HWND m_hwnd{nullptr};
    uint32_t m_width{0};
    uint32_t m_height{0};
    bool m_should_close{false};
};

} // namespace tinyrhi_examples
