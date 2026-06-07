#include "test_framework.h"

#include "TinyRHI/backend_factory.h"

#include <array>
#include <chrono>
#include <memory>
#include <thread>

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
        constexpr const char* kClassName = "TinyRHITimestampQueryTestWindow";
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

class OpenGLTestContext final {
public:
    ~OpenGLTestContext() { shutdown(); }

    OpenGLTestContext() = default;
    OpenGLTestContext(const OpenGLTestContext&) = delete;
    OpenGLTestContext& operator=(const OpenGLTestContext&) = delete;

    bool initialize()
    {
        if (!window.create(64, 64)) {
            return false;
        }

        instance = BackendFactory::createInstance(BackendType::OpenGL);
        if (instance == nullptr || !instance->init()) {
            shutdown();
            return false;
        }

        surface = instance->createSurface(window.nativeWindow());
        device = instance->getDevice();
        if (!surface || device == nullptr) {
            shutdown();
            return false;
        }

        swapchain = device->createSwapchain(surface, SwapchainDesc{
                                                         .enable_depth_stencil = false,
                                                         .vsync = false,
                                                     });
        if (!swapchain) {
            shutdown();
            return false;
        }

        return true;
    }

    void shutdown()
    {
        if (device != nullptr && swapchain) {
            device->destroySwapchain(swapchain);
            swapchain = {};
        }

        if (instance != nullptr && surface) {
            instance->destroySurface(surface);
            surface = {};
        }

        if (instance != nullptr) {
            instance->shutdown();
            instance.reset();
        }

        device = nullptr;
        window.destroy();
    }

    HiddenWin32Window window;
    std::unique_ptr<Instance> instance;
    Device* device{nullptr};
    SurfaceHandle surface{};
    SwapchainHandle swapchain{};
};

bool waitForTimestampResults(Device& device,
                             TimestampQueryPoolHandle pool,
                             uint32_t count,
                             uint64_t* timestamps_ns)
{
    for (uint32_t attempt = 0; attempt < 500; ++attempt) {
        if (device.getTimestampQueryResults(pool, 0, count, timestamps_ns)) {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return false;
}

#endif

} // namespace

TINYRHI_TEST_CASE("OpenGL timestamp query pool creates and destroys")
{
#if defined(_WIN32)
    OpenGLTestContext context;
    if (!context.initialize()) {
        return;
    }

    TimestampQueryPoolHandle pool = context.device->createTimestampQueryPool(TimestampQueryPoolDesc{.count = 2});
    TINYRHI_REQUIRE(!!pool);

    std::array<uint64_t, 2> timestamps{};
    TINYRHI_CHECK(!context.device->getTimestampQueryResults(pool, 0, 2, timestamps.data()));

    context.device->destroyTimestampQueryPool(pool);
    TINYRHI_CHECK(!context.device->getTimestampQueryResults(pool, 0, 1, timestamps.data()));
#endif
}

TINYRHI_TEST_CASE("OpenGL timestamp queries are non-blocking and return nanoseconds")
{
#if defined(_WIN32)
    OpenGLTestContext context;
    if (!context.initialize()) {
        return;
    }

    Device& device = *context.device;
    const TimestampQueryPoolHandle pool = device.createTimestampQueryPool(TimestampQueryPoolDesc{.count = 2});
    TINYRHI_REQUIRE(!!pool);

    const CommandListHandle commandListHandle = device.createCommandList();
    TINYRHI_REQUIRE(!!commandListHandle);
    auto* commandList = device.getCommandList(commandListHandle);
    TINYRHI_REQUIRE(commandList != nullptr);

    const TextureHandle renderTarget = device.createTexture(TextureDesc{
        .width = 16,
        .height = 16,
        .format = TextureFormat::RGBA8_UNorm,
        .usage = TextureUsage::RenderTarget,
    });
    TINYRHI_REQUIRE(!!renderTarget);

    const TextureViewHandle renderTargetView = device.createTextureView(TextureViewDesc{
        .texture = renderTarget,
        .view_dimension = TextureViewDimension::Texture2D,
        .format = TextureFormat::RGBA8_UNorm,
        .aspect = TextureAspect::Color,
    });
    TINYRHI_REQUIRE(!!renderTargetView);

    std::array<uint64_t, 2> timestamps{};
    commandList->begin();
    commandList->resetTimestampQueries(pool, 0, 2);
    commandList->end();
    TINYRHI_CHECK(!device.getTimestampQueryResults(pool, 0, 2, timestamps.data()));

    RenderPassBeginInfo pass{};
    pass.color_attachments.push_back(ColorAttachmentDesc{
        .view = renderTargetView,
        .load_op = LoadOp::Clear,
        .store_op = StoreOp::Store,
        .clear_color = ClearColor{0.1f, 0.2f, 0.3f, 1.0f},
    });
    pass.width = 16;
    pass.height = 16;

    commandList->begin();
    commandList->resetTimestampQueries(pool, 0, 2);
    commandList->writeTimestamp(pool, 0);
    commandList->beginRenderPass(pass);
    commandList->endRenderPass();
    commandList->writeTimestamp(pool, 1);
    commandList->end();
    device.submit(commandListHandle);

    TINYRHI_REQUIRE(waitForTimestampResults(device, pool, 2, timestamps.data()));
    TINYRHI_CHECK(timestamps[0] != 0);
    TINYRHI_CHECK(timestamps[1] >= timestamps[0]);

    device.destroyTextureView(renderTargetView);
    device.destroyTexture(renderTarget);
    device.destroyCommandList(commandListHandle);
    device.destroyTimestampQueryPool(pool);
#endif
}
