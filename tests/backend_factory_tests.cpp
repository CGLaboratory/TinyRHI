#include "test_framework.h"

#include "TinyRHI/backend_factory.h"

using namespace lunalite::rhi;

namespace {

class TestSurface final : public Surface {
public:
    explicit TestSurface(NativeSurfaceHandle native)
        : m_native(native)
    {}

    NativeSurfaceHandle getNativeHandle() const override
    {
        return m_native;
    }

    uint32_t getWidth() const override
    {
        return m_width;
    }

    uint32_t getHeight() const override
    {
        return m_height;
    }

    void resize(uint32_t width, uint32_t height) override
    {
        m_width = width;
        m_height = height;
    }

private:
    NativeSurfaceHandle m_native{};
    uint32_t m_width{640};
    uint32_t m_height{480};
};

} // namespace

TINYRHI_TEST_CASE("backend factory creates the OpenGL backend")
{
    auto instance = BackendFactory::createInstance(BackendType::OpenGL);

    TINYRHI_REQUIRE(instance != nullptr);
    TINYRHI_CHECK(instance->getBackendType() == BackendType::OpenGL);

    TINYRHI_CHECK(instance->init());
    TINYRHI_CHECK(instance->getDevice() != nullptr);
    instance->shutdown();
}

TINYRHI_TEST_CASE("backend factory rejects unavailable backends")
{
    TINYRHI_CHECK(BackendFactory::createInstance(BackendType::Vulkan) == nullptr);
    TINYRHI_CHECK(BackendFactory::createInstance(BackendType::D3D12) == nullptr);
    TINYRHI_CHECK(BackendFactory::createInstance(BackendType::Metal) == nullptr);
}

TINYRHI_TEST_CASE("OpenGL device rejects invalid swapchain surfaces")
{
    auto instance = BackendFactory::createInstance(BackendType::OpenGL);
    TINYRHI_REQUIRE(instance != nullptr);
    TINYRHI_REQUIRE(instance->init());

    TestSurface surface(NativeSurfaceHandle{});
    auto* device = instance->getDevice();
    TINYRHI_REQUIRE(device != nullptr);
    TINYRHI_CHECK(device->createSwapchain(surface, SwapchainDesc{}) == 0);
    TINYRHI_CHECK(device->getSwapchain(1) == nullptr);
    instance->shutdown();
}
