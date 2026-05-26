#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace tinyrhi_examples {

class Win32ImGuiPlatform final {
public:
    Win32ImGuiPlatform() = default;
    ~Win32ImGuiPlatform();

    Win32ImGuiPlatform(const Win32ImGuiPlatform&) = delete;
    Win32ImGuiPlatform& operator=(const Win32ImGuiPlatform&) = delete;

    bool init(HWND hwnd);
    void shutdown();
    void newFrame();

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    HWND m_hwnd{nullptr};
    WNDPROC m_previous_proc{nullptr};
};

} // namespace tinyrhi_examples
