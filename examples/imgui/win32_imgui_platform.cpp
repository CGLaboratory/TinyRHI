#include "win32_imgui_platform.h"

#include <imgui.h>
#include <backends/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

namespace tinyrhi_examples {
namespace {

constexpr const char* kPlatformPropertyName = "TinyRHIImGuiPlatform";

} // namespace

Win32ImGuiPlatform::~Win32ImGuiPlatform()
{
    shutdown();
}

bool Win32ImGuiPlatform::init(HWND hwnd)
{
    if (hwnd == nullptr || m_hwnd != nullptr) {
        return false;
    }

    if (!ImGui_ImplWin32_InitForOpenGL(hwnd)) {
        return false;
    }

    SetPropA(hwnd, kPlatformPropertyName, this);
    SetLastError(0);
    auto* previousProc =
        reinterpret_cast<WNDPROC>(SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&windowProc)));
    if (previousProc == nullptr && GetLastError() != 0) {
        RemovePropA(hwnd, kPlatformPropertyName);
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    m_hwnd = hwnd;
    m_previous_proc = previousProc;
    return true;
}

void Win32ImGuiPlatform::shutdown()
{
    if (m_hwnd == nullptr) {
        return;
    }

    if (m_previous_proc != nullptr) {
        SetWindowLongPtrA(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_previous_proc));
    }

    RemovePropA(m_hwnd, kPlatformPropertyName);
    ImGui_ImplWin32_Shutdown();
    m_previous_proc = nullptr;
    m_hwnd = nullptr;
}

void Win32ImGuiPlatform::newFrame()
{
    ImGui_ImplWin32_NewFrame();
}

LRESULT CALLBACK Win32ImGuiPlatform::windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam)) {
        return TRUE;
    }

    auto* platform = static_cast<Win32ImGuiPlatform*>(GetPropA(hwnd, kPlatformPropertyName));
    if (platform != nullptr && platform->m_previous_proc != nullptr) {
        return CallWindowProcA(platform->m_previous_proc, hwnd, message, wparam, lparam);
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

} // namespace tinyrhi_examples
