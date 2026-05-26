#pragma once
#include "device.h"
#include "rhi_types.h"

namespace lunalite::rhi {
class Instance {
public:
    virtual ~Instance() = default;

    virtual BackendType getBackendType() const = 0;

    virtual bool init() = 0;
    virtual void shutdown() = 0;
    virtual Device* getDevice() = 0;
};
} // namespace lunalite::rhi
