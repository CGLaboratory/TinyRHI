#include "TinyRHI/backend_factory.h"
#include "common/win32_gl_surface.h"

#include <chrono>
#include <cstdio>
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
    if (!surface.create("TinyRHI Clear Window", 960, 540, instance->getWindowRequirements())) {
        std::printf("Failed to create Win32 OpenGL surface.\n");
        return 1;
    }

    if (!instance->init(surface)) {
        std::printf("Failed to initialize TinyRHI instance.\n");
        return 1;
    }

    auto* swapchain = instance->getSwapchain();
    auto& commands = instance->getDevice()->getCommandList();

    while (surface.pollEvents() && !surface.shouldClose()) {
        swapchain->resize(surface.getWidth(), surface.getHeight());

        RenderPassBeginInfo pass{};
        pass.color_attachments.push_back(ColorAttachmentDesc{
            .view = swapchain->getCurrentColorTextureView(),
            .load_op = LoadOp::Clear,
            .store_op = StoreOp::Store,
            .clear_color = ClearColor{0.08f, 0.12f, 0.16f, 1.0f},
        });
        pass.width = swapchain->getWidth();
        pass.height = swapchain->getHeight();

        commands.begin();
        commands.beginRenderPass(pass);
        commands.endRenderPass();
        commands.end();

        swapchain->present();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    instance->shutdown();
    return 0;
}
