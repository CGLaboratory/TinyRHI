#include "test_framework.h"

#include "TinyRHI/backend_factory.h"

using namespace lunalite::rhi;

namespace {

class TestSurface final : public Surface {
public:
    explicit TestSurface(SurfaceDesc desc)
        : m_desc(desc)
    {}

    const SurfaceDesc& getSurfaceDesc() const override
    {
        return m_desc;
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
    SurfaceDesc m_desc{};
    uint32_t m_width{640};
    uint32_t m_height{480};
};

} // namespace

TINYRHI_TEST_CASE("backend factory creates the OpenGL backend")
{
    auto instance = BackendFactory::createInstance(BackendType::OpenGL);

    TINYRHI_REQUIRE(instance != nullptr);
    TINYRHI_CHECK(instance->getBackendType() == BackendType::OpenGL);

    const WindowRequirements requirements = instance->getWindowRequirements();
    TINYRHI_CHECK(requirements.backend == BackendType::OpenGL);
    TINYRHI_CHECK(requirements.glMajor == 4);
    TINYRHI_CHECK(requirements.glMinor == 5);
    TINYRHI_CHECK(requirements.gl_core_profile);
}

TINYRHI_TEST_CASE("backend factory rejects unavailable backends")
{
    TINYRHI_CHECK(BackendFactory::createInstance(BackendType::Vulkan) == nullptr);
    TINYRHI_CHECK(BackendFactory::createInstance(BackendType::D3D12) == nullptr);
    TINYRHI_CHECK(BackendFactory::createInstance(BackendType::Metal) == nullptr);
}

TINYRHI_TEST_CASE("OpenGL instance rejects non OpenGL surfaces")
{
    auto instance = BackendFactory::createInstance(BackendType::OpenGL);
    TINYRHI_REQUIRE(instance != nullptr);

    SurfaceDesc desc{};
    desc.backend = BackendType::Vulkan;
    desc.kind = SurfaceKind::NativeWindow;

    TestSurface surface(desc);
    TINYRHI_CHECK(!instance->init(surface));
    TINYRHI_CHECK(instance->getDevice() == nullptr);
    TINYRHI_CHECK(instance->getSwapchain() == nullptr);
}

TINYRHI_TEST_CASE("OpenGL instance rejects OpenGL surfaces without required callbacks")
{
    auto instance = BackendFactory::createInstance(BackendType::OpenGL);
    TINYRHI_REQUIRE(instance != nullptr);

    SurfaceDesc desc{};
    desc.backend = BackendType::OpenGL;
    desc.kind = SurfaceKind::OpenGLContext;

    TestSurface surface(desc);
    TINYRHI_CHECK(!instance->init(surface));
    TINYRHI_CHECK(instance->getDevice() == nullptr);
    TINYRHI_CHECK(instance->getSwapchain() == nullptr);
}
