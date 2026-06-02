#pragma once

namespace lunalite::rhi {

enum class FilterMode {
    Nearest,
    Linear
};

enum class MipFilter {
    None,
    Nearest,
    Linear
};

enum class AddressMode {
    Repeat,
    ClampToEdge,
    MirroredRepeat
};

struct SamplerDesc {
    FilterMode min_filter{FilterMode::Linear};
    FilterMode mag_filter{FilterMode::Linear};
    MipFilter mip_filter{MipFilter::None};
    AddressMode address_u{AddressMode::ClampToEdge};
    AddressMode address_v{AddressMode::ClampToEdge};
    AddressMode address_w{AddressMode::ClampToEdge};
};

} // namespace lunalite::rhi
