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

    bool init() override;
    void shutdown() override;
    Device* getDevice() override;

private:
    std::unique_ptr<Device> m_device;
};

} // namespace lunalite::rhi
