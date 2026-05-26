#pragma once
#include "../interface/instance.h"

#include <memory>
#include <vector>

namespace lunalite::rhi {

class OpenGLInstance final : public Instance {
public:
    ~OpenGLInstance() override;

    BackendType getBackendType() const override
    {
        return BackendType::OpenGL;
    }

    bool init() override;
    void shutdown() override;
    SurfaceHandle createSurface(const NativeWindowHandle& native_window) override;
    void destroySurface(SurfaceHandle surface) override;
    Surface* getSurface(SurfaceHandle surface) override;
    Device* getDevice() override;

private:
    Surface* getOpenGLSurface(SurfaceHandle handle);

    std::unique_ptr<Device> m_device;
    std::vector<std::unique_ptr<Surface>> m_surfaces;
};

} // namespace lunalite::rhi
