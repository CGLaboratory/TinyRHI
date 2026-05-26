#include "test_framework.h"

#include "TinyRHI/backend_factory.h"

using namespace lunalite::rhi;

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

TINYRHI_TEST_CASE("OpenGL instance rejects invalid native windows")
{
    auto instance = BackendFactory::createInstance(BackendType::OpenGL);
    TINYRHI_REQUIRE(instance != nullptr);
    TINYRHI_REQUIRE(instance->init());

    TINYRHI_CHECK(instance->createSurface(NativeWindowHandle{}) == 0);
    TINYRHI_CHECK(instance->getSurface(1) == nullptr);
    instance->shutdown();
}

TINYRHI_TEST_CASE("OpenGL device rejects invalid surface handles")
{
    auto instance = BackendFactory::createInstance(BackendType::OpenGL);
    TINYRHI_REQUIRE(instance != nullptr);
    TINYRHI_REQUIRE(instance->init());

    auto* device = instance->getDevice();
    TINYRHI_REQUIRE(device != nullptr);
    TINYRHI_CHECK(device->createSwapchain(1, SwapchainDesc{}) == 0);
    TINYRHI_CHECK(device->getSwapchain(1) == nullptr);
    instance->shutdown();
}
