#pragma once
#include "../interface/instance.h"

#include <memory>

namespace lunalite::rhi {

class OpenGLInstance final : public Instance {
public:
    ~OpenGLInstance() override;

    BackendType getBackendType() const override
    {
        return BackendType::OpenGL;
    }

    WindowRequirements getWindowRequirements() const override
    {
        WindowRequirements req{.backend = BackendType::OpenGL,
                               .glMajor = 4,
                               .glMinor = 5,
                               .gl_core_profile = true,
                               .gl_debug_context = true};

        return req;
    }

    bool init(Surface& surface) override;
    void shutdown() override;
    void resize(uint32_t width, uint32_t height) override;
    Device* getDevice() override;
    Swapchain* getSwapchain() override;

private:
    Surface* m_surface{nullptr};
    std::unique_ptr<Device> m_device;
    std::unique_ptr<Swapchain> m_swapchain;
};

} // namespace lunalite::rhi
