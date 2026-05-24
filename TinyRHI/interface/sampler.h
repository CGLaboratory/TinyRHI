#pragma once

namespace lunalite::rhi {

enum class FilterMode {
    Nearest,
    Linear
};

enum class AddressMode {
    Repeat,
    ClampToEdge
};

struct SamplerDesc {
    FilterMode min_filter{FilterMode::Linear};
    FilterMode mag_filter{FilterMode::Linear};
    AddressMode address_u{AddressMode::ClampToEdge};
    AddressMode address_v{AddressMode::ClampToEdge};
};

} // namespace lunalite::rhi
