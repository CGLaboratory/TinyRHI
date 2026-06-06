#include "test_framework.h"

#include "TinyRHI/backend_factory.h"
#include "TinyRHI/opengl/device.h"

#include <cstdint>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace lunalite::rhi;

namespace {

#if defined(_WIN32)

class HiddenWin32Window final {
public:
    ~HiddenWin32Window() { destroy(); }

    HiddenWin32Window() = default;
    HiddenWin32Window(const HiddenWin32Window&) = delete;
    HiddenWin32Window& operator=(const HiddenWin32Window&) = delete;

    bool create(uint32_t width, uint32_t height)
    {
        constexpr const char* kClassName = "TinyRHITextureArrayTestWindow";
        m_instance = GetModuleHandleA(nullptr);

        WNDCLASSEXA wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = m_instance;
        wc.lpszClassName = kClassName;
        if (!RegisterClassExA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }

        m_hwnd = CreateWindowExA(0,
                                 kClassName,
                                 "",
                                 WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 static_cast<int>(width),
                                 static_cast<int>(height),
                                 nullptr,
                                 nullptr,
                                 m_instance,
                                 nullptr);
        return m_hwnd != nullptr;
    }

    void destroy()
    {
        if (m_hwnd != nullptr) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    NativeWindowHandle nativeWindow() const
    {
        return NativeWindowHandle{
            .platform = NativeWindowHandle::Platform::Win32,
            .display = nullptr,
            .window = m_hwnd,
        };
    }

private:
    HINSTANCE m_instance{nullptr};
    HWND m_hwnd{nullptr};
};

#endif

} // namespace

TINYRHI_TEST_CASE("OpenGL texture 2D array supports CSM depth views and sampled bind group")
{
#if defined(_WIN32)
    HiddenWin32Window window;
    if (!window.create(64, 64)) {
        return;
    }

    auto instance = BackendFactory::createInstance(BackendType::OpenGL);
    TINYRHI_REQUIRE(instance != nullptr);
    TINYRHI_REQUIRE(instance->init());

    const SurfaceHandle surface = instance->createSurface(window.nativeWindow());
    TINYRHI_REQUIRE(!!surface);

    auto* device = instance->getDevice();
    TINYRHI_REQUIRE(device != nullptr);
    auto* glDevice = dynamic_cast<OpenGLDevice*>(device);
    TINYRHI_REQUIRE(glDevice != nullptr);

    const SwapchainHandle swapchain = device->createSwapchain(surface, SwapchainDesc{
                                                                          .enable_depth_stencil = false,
                                                                          .vsync = false,
                                                                      });
    if (!swapchain) {
        instance->shutdown();
        return;
    }

    constexpr uint32_t kShadowMapSize = 128;
    constexpr uint32_t kCascadeCount = 4;
    const TextureHandle shadowMap = device->createTexture(TextureDesc{
        .width = kShadowMapSize,
        .height = kShadowMapSize,
        .dimension = TextureDimension::Texture2D,
        .format = TextureFormat::Depth32F,
        .usage = TextureUsage::DepthStencil | TextureUsage::Sampled,
        .mip_levels = 1,
        .array_layers = kCascadeCount,
    });
    TINYRHI_REQUIRE(!!shadowMap);

    std::vector<TextureViewHandle> layerViews;
    layerViews.reserve(kCascadeCount);
    for (uint32_t layer = 0; layer < kCascadeCount; ++layer) {
        TextureViewHandle layerView = device->createTextureView(TextureViewDesc{
            .texture = shadowMap,
            .view_dimension = TextureViewDimension::Texture2D,
            .format = TextureFormat::Depth32F,
            .aspect = TextureAspect::Depth,
            .base_array_layer = layer,
            .array_layer_count = 1,
        });
        TINYRHI_REQUIRE(!!layerView);
        layerViews.push_back(layerView);
    }

    const TextureViewHandle arrayView = device->createTextureView(TextureViewDesc{
        .texture = shadowMap,
        .view_dimension = TextureViewDimension::Texture2DArray,
        .format = TextureFormat::Depth32F,
        .aspect = TextureAspect::Depth,
        .array_layer_count = kCascadeCount,
    });
    TINYRHI_REQUIRE(!!arrayView);

    const SamplerHandle sampler = device->createSampler(SamplerDesc{});
    TINYRHI_REQUIRE(!!sampler);

    BindGroupLayoutDesc layoutDesc{};
    layoutDesc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .stages = shaderStageFlag(ShaderStage::Fragment),
    });
    const BindGroupLayoutHandle layout = device->createBindGroupLayout(layoutDesc);
    TINYRHI_REQUIRE(!!layout);

    BindGroupDesc groupDesc{};
    groupDesc.layout = layout;
    groupDesc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .texture_view = arrayView,
        .sampler = sampler,
    });
    const BindGroupHandle bindGroup = device->createBindGroup(groupDesc);
    TINYRHI_REQUIRE(!!bindGroup);

    const CommandListHandle commandListHandle = device->createCommandList();
    TINYRHI_REQUIRE(!!commandListHandle);
    auto* commandList = device->getCommandList(commandListHandle);
    TINYRHI_REQUIRE(commandList != nullptr);

    for (const TextureViewHandle layerView : layerViews) {
        RenderPassBeginInfo pass{};
        pass.has_depth_stencil_attachment = true;
        pass.depth_stencil_attachment.view = layerView;
        pass.depth_stencil_attachment.depth_load_op = LoadOp::Clear;
        pass.depth_stencil_attachment.clear_depth = 1.0f;
        pass.width = kShadowMapSize;
        pass.height = kShadowMapSize;

        TINYRHI_REQUIRE(glDevice->getFramebuffer(pass) != 0);

        commandList->begin();
        commandList->beginRenderPass(pass);
        commandList->endRenderPass();
        commandList->end();
        device->submit(commandListHandle);
    }

    TextureTransition shaderRead{
        .texture = shadowMap,
        .state = ResourceState::ShaderRead,
        .range =
            TextureSubresourceRange{
                .aspect = TextureAspect::Depth,
                .array_layer_count = kCascadeCount,
            },
    };
    commandList->begin();
    commandList->transition(&shaderRead, 1);
    commandList->setBindGroup(0, bindGroup);
    commandList->end();
    device->submit(commandListHandle);

    device->destroyCommandList(commandListHandle);
    device->destroyBindGroup(bindGroup);
    device->destroyBindGroupLayout(layout);
    device->destroySampler(sampler);
    device->destroyTextureView(arrayView);
    for (TextureViewHandle layerView : layerViews) {
        device->destroyTextureView(layerView);
    }
    device->destroyTexture(shadowMap);
    device->destroySwapchain(swapchain);
    instance->destroySurface(surface);
    instance->shutdown();
#endif
}
