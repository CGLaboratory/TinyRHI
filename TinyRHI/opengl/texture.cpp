#include "device.h"
#include "gl_convert.h"

#include <cstddef>

namespace lunalite::rhi {
namespace {
size_t textureFormatBytesPerPixel(TextureFormat format)
{
    switch (format) {
        case TextureFormat::RGBA8:
            return 4;
        case TextureFormat::RGBA16F:
            return 8;
        case TextureFormat::RGBA32F:
            return 16;
        case TextureFormat::Depth24Stencil8:
            return 4;
        case TextureFormat::Depth32F:
            return 4;
    }

    return 4;
}
} // namespace

TextureHandle OpenGLDevice::createTexture(const TextureDesc& desc)
{
    GLuint texture = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureStorage2D(texture,
                       static_cast<GLsizei>(desc.mip_levels),
                       toGLTextureInternalFormat(desc.format),
                       static_cast<GLsizei>(desc.width),
                       static_cast<GLsizei>(desc.height));

    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_textures.push_back(OpenGLTexture{.id = texture, .desc = desc, .is_swapchain_backbuffer = false});
    return static_cast<TextureHandle>(m_textures.size());
}

void OpenGLDevice::updateTexture(TextureHandle texture, const TextureUploadDesc& desc)
{
    auto* glTexture = getTexture(texture);
    if (glTexture == nullptr || glTexture->is_swapchain_backbuffer || desc.data == nullptr || desc.width == 0 ||
        desc.height == 0 || desc.x + desc.width > glTexture->desc.width || desc.y + desc.height > glTexture->desc.height) {
        return;
    }

    const auto bytesPerPixel = textureFormatBytesPerPixel(desc.format);
    const auto rowPitch = desc.row_pitch == 0 ? static_cast<size_t>(desc.width) * bytesPerPixel : desc.row_pitch;
    if (rowPitch % bytesPerPixel != 0) {
        return;
    }

    GLint previousAlignment = 4;
    GLint previousRowLength = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &previousRowLength);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(rowPitch / bytesPerPixel));
    glTextureSubImage2D(glTexture->id,
                        0,
                        static_cast<GLint>(desc.x),
                        static_cast<GLint>(desc.y),
                        static_cast<GLsizei>(desc.width),
                        static_cast<GLsizei>(desc.height),
                        toGLTextureUploadFormat(desc.format),
                        toGLTextureUploadType(desc.format),
                        desc.data);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, previousRowLength);
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
}

void OpenGLDevice::destroyTexture(TextureHandle texture)
{
    auto* glTexture = getTexture(texture);
    if (glTexture == nullptr || glTexture->is_swapchain_backbuffer) {
        return;
    }

    glDeleteTextures(1, &glTexture->id);
    glTexture->id = 0;
}

TextureViewHandle OpenGLDevice::createTextureView(const TextureViewDesc& desc)
{
    auto* glTexture = getTexture(desc.texture);
    if (glTexture == nullptr) {
        return 0;
    }

    m_texture_views.push_back(OpenGLTextureView{
        .texture = desc.texture,
        .format = desc.format,
        .aspect = desc.aspect,
        .base_mip_level = desc.base_mip_level,
        .mip_level_count = desc.mip_level_count,
        .base_array_layer = desc.base_array_layer,
        .array_layer_count = desc.array_layer_count,
    });
    return static_cast<TextureViewHandle>(m_texture_views.size());
}

void OpenGLDevice::destroyTextureView(TextureViewHandle view)
{
    auto* glView = getTextureView(view);
    if (glView == nullptr) {
        return;
    }

    glView->texture = 0;
}

TextureViewHandle OpenGLDevice::createSwapchainTextureView(TextureFormat format)
{
    const auto usage = isDepthFormat(format) ? TextureUsage::DepthStencil : TextureUsage::RenderTarget;

    m_textures.push_back(OpenGLTexture{
        .id = 0,
        .desc = TextureDesc{.width = 1, .height = 1, .format = format, .usage = usage},
        .is_swapchain_backbuffer = true,
    });

    const auto texture = static_cast<TextureHandle>(m_textures.size());
    m_texture_views.push_back(OpenGLTextureView{
        .texture = texture,
        .format = format,
        .aspect = isDepthFormat(format) ? TextureAspect::DepthStencil : TextureAspect::Color,
    });
    return static_cast<TextureViewHandle>(m_texture_views.size());
}

} // namespace lunalite::rhi
