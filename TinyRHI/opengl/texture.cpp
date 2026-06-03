#include "device.h"
#include "gl_convert.h"

#include <cstddef>

namespace lunalite::rhi {
namespace {
size_t textureFormatBytesPerPixel(TextureFormat format)
{
    switch (format) {
        case TextureFormat::RGBA8_UNorm:
        case TextureFormat::RGBA8_SRGB:
            return 4;
        case TextureFormat::RG16F:
            return 4;
        case TextureFormat::RG32F:
            return 8;
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

uint32_t textureMipDimension(uint32_t baseDimension, uint32_t mipLevel)
{
    uint32_t dimension = baseDimension;
    for (uint32_t level = 0; level < mipLevel && dimension > 1; ++level) {
        dimension /= 2;
    }

    return dimension == 0 ? 1 : dimension;
}
} // namespace

TextureHandle OpenGLDevice::createTexture(const TextureDesc& desc)
{
    const TextureDesc normalizedDesc = normalizeTextureDesc(desc);
    if (!textureDescValid(normalizedDesc)) {
        return {};
    }

    GLuint texture = 0;
    glCreateTextures(toGLTextureTarget(normalizedDesc.dimension), 1, &texture);
    glTextureStorage2D(texture,
                       static_cast<GLsizei>(normalizedDesc.mip_levels),
                       toGLTextureInternalFormat(normalizedDesc.format),
                       static_cast<GLsizei>(normalizedDesc.width),
                       static_cast<GLsizei>(normalizedDesc.height));

    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    m_textures.push_back(OpenGLTexture{.id = texture, .desc = normalizedDesc, .is_swapchain_backbuffer = false});
    return makeHandle<TextureHandle>(m_textures.size() - 1);
}

void OpenGLDevice::updateTexture(TextureHandle texture, const TextureUploadDesc& desc)
{
    auto* glTexture = getTexture(texture);
    if (glTexture == nullptr || glTexture->is_swapchain_backbuffer || desc.data == nullptr || desc.width == 0 ||
        desc.height == 0 || desc.mip_level >= glTexture->desc.mip_levels ||
        desc.array_layer >= glTexture->desc.array_layers) {
        return;
    }

    const uint32_t mipWidth = textureMipDimension(glTexture->desc.width, desc.mip_level);
    const uint32_t mipHeight = textureMipDimension(glTexture->desc.height, desc.mip_level);
    if (desc.x > mipWidth || desc.y > mipHeight || desc.width > mipWidth - desc.x || desc.height > mipHeight - desc.y) {
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
    if (glTexture->desc.dimension == TextureDimension::TextureCube) {
        glTextureSubImage3D(glTexture->id,
                            static_cast<GLint>(desc.mip_level),
                            static_cast<GLint>(desc.x),
                            static_cast<GLint>(desc.y),
                            static_cast<GLint>(desc.array_layer),
                            static_cast<GLsizei>(desc.width),
                            static_cast<GLsizei>(desc.height),
                            1,
                            toGLTextureUploadFormat(desc.format),
                            toGLTextureUploadType(desc.format),
                            desc.data);
    } else {
        glTextureSubImage2D(glTexture->id,
                            static_cast<GLint>(desc.mip_level),
                            static_cast<GLint>(desc.x),
                            static_cast<GLint>(desc.y),
                            static_cast<GLsizei>(desc.width),
                            static_cast<GLsizei>(desc.height),
                            toGLTextureUploadFormat(desc.format),
                            toGLTextureUploadType(desc.format),
                            desc.data);
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, previousRowLength);
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
}

void OpenGLDevice::generateMipmaps(TextureHandle texture)
{
    auto* glTexture = getTexture(texture);
    if (glTexture == nullptr || glTexture->is_swapchain_backbuffer || isDepthFormat(glTexture->desc.format)) {
        return;
    }

    glGenerateTextureMipmap(glTexture->id);
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
    if (glTexture == nullptr || !textureViewDescValid(glTexture->desc, desc)) {
        return {};
    }

    m_texture_views.push_back(OpenGLTextureView{
        .texture = desc.texture,
        .view_dimension = desc.view_dimension,
        .format = desc.format,
        .aspect = desc.aspect,
        .base_mip_level = desc.base_mip_level,
        .mip_level_count = desc.mip_level_count,
        .base_array_layer = desc.base_array_layer,
        .array_layer_count = desc.array_layer_count,
    });
    return makeHandle<TextureViewHandle>(m_texture_views.size() - 1);
}

void OpenGLDevice::destroyTextureView(TextureViewHandle view)
{
    auto* glView = getTextureView(view);
    if (glView == nullptr) {
        return;
    }

    glView->texture = {};
}

TextureViewHandle OpenGLDevice::createSwapchainTextureView(TextureFormat format, SwapchainHandle swapchain)
{
    const auto usage = isDepthFormat(format) ? TextureUsage::DepthStencil : TextureUsage::RenderTarget;

    m_textures.push_back(OpenGLTexture{
        .id = 0,
        .desc = TextureDesc{.width = 1, .height = 1, .format = format, .usage = usage},
        .is_swapchain_backbuffer = true,
        .swapchain = swapchain,
    });

    const auto texture = makeHandle<TextureHandle>(m_textures.size() - 1);
    m_texture_views.push_back(OpenGLTextureView{
        .texture = texture,
        .format = format,
        .aspect = isDepthFormat(format) ? TextureAspect::DepthStencil : TextureAspect::Color,
    });
    return makeHandle<TextureViewHandle>(m_texture_views.size() - 1);
}

void OpenGLDevice::resizeSwapchainTextureView(TextureViewHandle view, uint32_t width, uint32_t height)
{
    auto* glView = getTextureView(view);
    auto* glTexture = glView ? getTexture(glView->texture) : nullptr;
    if (glTexture == nullptr || !glTexture->is_swapchain_backbuffer) {
        return;
    }

    glTexture->desc.width = width;
    glTexture->desc.height = height;
}

} // namespace lunalite::rhi
