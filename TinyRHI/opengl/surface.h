#pragma once

#include "../interface/surface.h"

namespace lunalite::rhi {

class OpenGLSurface final : public Surface {
public:
    explicit OpenGLSurface(const NativeWindowHandle& native);

    NativeWindowHandle getNativeHandle() const override;
    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    void resize(uint32_t width, uint32_t height) override;

private:
    NativeWindowHandle m_native{};
    uint32_t m_width{1};
    uint32_t m_height{1};
};

} // namespace lunalite::rhi
