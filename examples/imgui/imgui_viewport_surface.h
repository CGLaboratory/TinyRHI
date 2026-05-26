#pragma once

#include "TinyRHI/interface/surface.h"

#include <cstdint>

namespace tinyrhi_examples {

class ImGuiViewportSurface final : public lunalite::rhi::Surface {
public:
    ImGuiViewportSurface(void* window, uint32_t width, uint32_t height);

    lunalite::rhi::NativeSurfaceHandle getNativeHandle() const override;
    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    void resize(uint32_t width, uint32_t height) override;

private:
    void* m_window{nullptr};
    uint32_t m_width{0};
    uint32_t m_height{0};
};

} // namespace tinyrhi_examples
