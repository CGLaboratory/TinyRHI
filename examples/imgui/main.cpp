#include "common/win32_window.h"
#include "imgui_sample_app.h"
#include "tiny_rhi_imgui_renderer.h"
#include "TinyRHI/backend_factory.h"
#include "win32_imgui_platform.h"

#include <cstdio>

#include <chrono>
#include <imgui.h>
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

    tinyrhi_examples::Win32Window surface;
    if (!surface.create("TinyRHI ImGui", 1'280, 720)) {
        std::printf("Failed to create Win32 surface.\n");
        return 1;
    }

    if (!instance->init()) {
        std::printf("Failed to initialize TinyRHI instance.\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

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
    if (device == nullptr) {
        std::printf("TinyRHI device is unavailable.\n");
        renderer.shutdown();
        platform.shutdown();
        ImGui::DestroyContext();
        instance->shutdown();
        return 1;
    }

    const SurfaceHandle surfaceHandle = instance->createSurface(surface.nativeWindow());
    const SwapchainHandle swapchainHandle = device->createSwapchain(surfaceHandle, SwapchainDesc{});
    auto* swapchain = device->getSwapchain(swapchainHandle);
    renderer.setSurfaceOwner(*instance);
    if (!surfaceHandle || swapchain == nullptr || !renderer.init(*device)) {
        std::printf("Failed to initialize ImGui TinyRHI renderer backend.\n");
        renderer.shutdown();
        platform.shutdown();
        ImGui::DestroyContext();
        instance->shutdown();
        return 1;
    }

    const CommandListHandle commandListHandle = device->createCommandList();
    auto* commandList = device->getCommandList(commandListHandle);
    if (commandList == nullptr) {
        std::printf("Failed to create command list.\n");
        renderer.shutdown();
        platform.shutdown();
        ImGui::DestroyContext();
        instance->shutdown();
        return 1;
    }
    auto& commands = *commandList;

    while (surface.pollEvents() && !surface.shouldClose()) {
        platform.newFrame();
        ImGui::NewFrame();
        app.draw();
        ImGui::Render();

        swapchain->resize(surface.getWidth(), surface.getHeight());
        SwapchainFrame frame{};
        if (!device->beginFrame(swapchainHandle, frame)) {
            break;
        }

        RenderPassBeginInfo pass{};
        pass.color_attachments.push_back(ColorAttachmentDesc{
            .view = frame.color_view,
            .load_op = LoadOp::Clear,
            .store_op = StoreOp::Store,
            .clear_color = app.clearColor(),
        });
        pass.width = frame.width;
        pass.height = frame.height;

        commands.begin();
        commands.beginRenderPass(pass);
        renderer.render(ImGui::GetDrawData(), commands);
        commands.endRenderPass();
        commands.end();
        device->submit(commandListHandle, &frame);

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault(nullptr, &renderer);
        }

        device->present(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    renderer.shutdown();
    platform.shutdown();
    ImGui::DestroyContext();
    instance->shutdown();
    return 0;
}
