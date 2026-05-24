#pragma once

#include "TinyRHI/interface/surface.h"

#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace tinyrhi_examples {

class Win32GLSurface final : public lunalite::rhi::Surface {
public:
    Win32GLSurface() = default;
    ~Win32GLSurface() override;

    Win32GLSurface(const Win32GLSurface&) = delete;
    Win32GLSurface& operator=(const Win32GLSurface&) = delete;

    bool create(
        const char* title,
        uint32_t width,
        uint32_t height,
        const lunalite::rhi::WindowRequirements& requirements);
    void destroy();

    bool pollEvents();
    bool shouldClose() const;
    void requestClose();

    const lunalite::rhi::SurfaceDesc& getSurfaceDesc() const override;
    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    void resize(uint32_t width, uint32_t height) override;

    HWND hwnd() const;

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static bool makeCurrent(void* userData);
    static void* getProcAddress(void* userData, const char* name);
    static void swapBuffers(void* userData);
    static void setSwapInterval(void* userData, int interval);
    static void getFramebufferSize(void* userData, uint32_t& width, uint32_t& height);

    bool createWindow(const char* title, uint32_t width, uint32_t height);
    bool createContext(const lunalite::rhi::WindowRequirements& requirements);
    void handleResize(uint32_t width, uint32_t height);

    HINSTANCE m_instance{nullptr};
    HWND m_hwnd{nullptr};
    HDC m_dc{nullptr};
    HGLRC m_context{nullptr};
    uint32_t m_width{0};
    uint32_t m_height{0};
    bool m_should_close{false};
    lunalite::rhi::SurfaceDesc m_desc{};
};

} // namespace tinyrhi_examples
