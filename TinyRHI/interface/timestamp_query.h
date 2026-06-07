#pragma once
#include "rhi_types.h"

#include <cstdint>

namespace lunalite::rhi {

struct TimestampQueryPoolDesc {
    uint32_t count{0};
};

} // namespace lunalite::rhi
