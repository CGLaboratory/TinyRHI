#include "imgui_sample_app.h"

#include <imgui.h>

namespace tinyrhi_examples {

void ImGuiSampleApp::draw()
{
    ImGui::ShowDemoWindow(&m_show_demo_window);

    ImGui::Begin("TinyRHI ImGui");
    ImGui::Text("Renderer: TinyRHI");
    ImGui::Checkbox("Show demo window", &m_show_demo_window);
    ImGui::ColorEdit3("Clear color", m_clear_color);
    ImGui::Text("Frame %.3f ms (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}

lunalite::rhi::ClearColor ImGuiSampleApp::clearColor() const
{
    return lunalite::rhi::ClearColor{m_clear_color[0], m_clear_color[1], m_clear_color[2], m_clear_color[3]};
}

} // namespace tinyrhi_examples
