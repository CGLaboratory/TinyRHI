#include "backend_factory.h"
#include "opengl/instance.h"

namespace lunalite::rhi {
std::unique_ptr<Instance> BackendFactory::createInstance(BackendType type)
{
    switch (type) {
        case BackendType::OpenGL:
            return std::make_unique<OpenGLInstance>();
        default:
            return nullptr;
    }
}
} // namespace lunalite::rhi
