#include "imgui_sample_app.h"
#include "tiny_rhi_imgui_renderer.h"
#include "win32_imgui_platform.h"

#include "TinyRHI/backend_factory.h"
#include "common/win32_gl_surface.h"

#include <imgui.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

using namespace lunalite::rhi;

int main()
{
    auto instance = BackendFactory::createInstance(BackendType::OpenGL);
    if (!instance) {
        std::printf("Failed to create OpenGL backend.\n");
        return 1;
    }

    tinyrhi_examples::Win32GLSurface surface;
    if (!surface.create("TinyRHI ImGui", 1280, 720, instance->getWindowRequirements())) {
        std::printf("Failed to create Win32 OpenGL surface.\n");
        return 1;
    }

    if (!instance->init(surface)) {
        std::printf("Failed to initialize TinyRHI instance.\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    tinyrhi_examples::Win32ImGuiPlatform platform;
    tinyrhi_examples::TinyRHIImGuiRenderer renderer;
    tinyrhi_examples::ImGuiSampleApp app;

    if (!platform.init(surface.hwnd())) {
        std::printf("Failed to initialize ImGui Win32 platform backend.\n");
        ImGui::DestroyContext();
        instance->shutdown();
        return 1;
    }

    auto* device = instance->getDevice();
    auto* swapchain = instance->getSwapchain();
    if (device == nullptr || swapchain == nullptr || !renderer.init(*device)) {
        std::printf("Failed to initialize ImGui TinyRHI renderer backend.\n");
        renderer.shutdown();
        platform.shutdown();
        ImGui::DestroyContext();
        instance->shutdown();
        return 1;
    }

    auto& commands = device->getCommandList();

    while (surface.pollEvents() && !surface.shouldClose()) {
        platform.newFrame();
        ImGui::NewFrame();
        app.draw();
        ImGui::Render();

        swapchain->resize(surface.getWidth(), surface.getHeight());

        RenderPassBeginInfo pass{};
        pass.color_attachments.push_back(ColorAttachmentDesc{
            .view = swapchain->getCurrentColorTextureView(),
            .load_op = LoadOp::Clear,
            .store_op = StoreOp::Store,
            .clear_color = app.clearColor(),
        });
        pass.width = swapchain->getWidth();
        pass.height = swapchain->getHeight();

        commands.begin();
        commands.beginRenderPass(pass);
        renderer.render(ImGui::GetDrawData(), commands);
        commands.endRenderPass();
        commands.end();

        swapchain->present();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    renderer.shutdown();
    platform.shutdown();
    ImGui::DestroyContext();
    instance->shutdown();
    return 0;
}
