#pragma once
#include "interface/instance.h"
#include "interface/rhi_types.h"

#include <memory>

namespace lunalite::rhi {
class BackendFactory {
public:
    static std::unique_ptr<Instance> createInstance(BackendType type);
};
} // namespace lunalite::rhi
