#include "device.h"
#include "instance.h"

namespace lunalite::rhi {

OpenGLInstance::~OpenGLInstance() = default;

bool OpenGLInstance::init()
{
    m_device = std::make_unique<OpenGLDevice>();
    return true;
}

void OpenGLInstance::shutdown()
{
    m_device.reset();
}

Device* OpenGLInstance::getDevice()
{
    return m_device.get();
}

} // namespace lunalite::rhi
