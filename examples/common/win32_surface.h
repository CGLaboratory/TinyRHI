#pragma once

#include "TinyRHI/interface/surface.h"

#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace tinyrhi_examples {

class Win32Surface final : public lunalite::rhi::Surface {
public:
    Win32Surface() = default;
    ~Win32Surface() override;

    Win32Surface(const Win32Surface&) = delete;
    Win32Surface& operator=(const Win32Surface&) = delete;

    bool create(const char* title, uint32_t width, uint32_t height);
    void destroy();

    bool pollEvents();
    bool shouldClose() const;
    void requestClose();

    lunalite::rhi::NativeSurfaceHandle getNativeHandle() const override;
    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    void resize(uint32_t width, uint32_t height) override;

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
