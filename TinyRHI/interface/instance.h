#pragma once
#include "device.h"
#include "rhi_types.h"
#include "surface.h"
#include "swapchain.h"

namespace lunalite::rhi {
class Instance {
public:
    virtual ~Instance() = default;

    virtual BackendType getBackendType() const = 0;
    virtual WindowRequirements getWindowRequirements() const = 0;

    virtual bool init(Surface& surface) = 0;
    virtual void shutdown() = 0;
    virtual Device* getDevice() = 0;
    virtual Swapchain* getSwapchain() = 0;
};
} // namespace lunalite::rhi
