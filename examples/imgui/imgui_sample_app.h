#pragma once

#include "TinyRHI/interface/render_pass.h"

namespace tinyrhi_examples {

class ImGuiSampleApp final {
public:
    void draw();
    lunalite::rhi::ClearColor clearColor() const;

private:
    bool m_show_demo_window{true};
    float m_clear_color[4]{0.08f, 0.09f, 0.11f, 1.0f};
};

} // namespace tinyrhi_examples
