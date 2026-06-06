#include "device.h"
#include "gl_convert.h"

namespace lunalite::rhi {
namespace {
bool hasUsage(TextureUsage usage, TextureUsage required)
{
    return (usage & required) == required;
}

bool textureInitialStateCompatible(const TextureDesc& desc, ResourceState state, bool swapchainBackbuffer)
{
    switch (state) {
        case ResourceState::Undefined:
            return true;
        case ResourceState::CopySrc:
            return hasUsage(desc.usage, TextureUsage::CopySrc);
        case ResourceState::CopyDst:
            return hasUsage(desc.usage, TextureUsage::CopyDst);
        case ResourceState::ShaderRead:
            return hasUsage(desc.usage, TextureUsage::Sampled);
        case ResourceState::StorageRead:
        case ResourceState::StorageReadWrite:
            return hasUsage(desc.usage, TextureUsage::Storage) && !isDepthFormat(desc.format) &&
                   !isSRGBFormat(desc.format);
        case ResourceState::ColorAttachment:
            return hasUsage(desc.usage, TextureUsage::RenderTarget) && !isDepthFormat(desc.format);
        case ResourceState::DepthStencilRead:
        case ResourceState::DepthStencilWrite:
            return hasUsage(desc.usage, TextureUsage::DepthStencil) && isDepthFormat(desc.format);
        case ResourceState::Present:
            return swapchainBackbuffer;
        case ResourceState::VertexBuffer:
        case ResourceState::IndexBuffer:
        case ResourceState::IndirectArgument:
        case ResourceState::UniformRead:
            return false;
    }

    return false;
}
} // namespace

TextureHandle OpenGLDevice::createTexture(const TextureDesc& desc)
{
    const TextureDesc normalizedDesc = normalizeTextureDesc(desc);
    if (!textureDescValid(normalizedDesc) ||
        !textureInitialStateCompatible(normalizedDesc, normalizedDesc.initial_state, false)) {
        return {};
    }

    GLuint texture = 0;
    glCreateTextures(toGLTextureTarget(normalizedDesc), 1, &texture);
    if (normalizedDesc.dimension == TextureDimension::Texture2D && normalizedDesc.array_layers > 1) {
        glTextureStorage3D(texture,
                           static_cast<GLsizei>(normalizedDesc.mip_levels),
                           toGLTextureInternalFormat(normalizedDesc.format),
                           static_cast<GLsizei>(normalizedDesc.width),
                           static_cast<GLsizei>(normalizedDesc.height),
                           static_cast<GLsizei>(normalizedDesc.array_layers));
    } else {
        glTextureStorage2D(texture,
                           static_cast<GLsizei>(normalizedDesc.mip_levels),
                           toGLTextureInternalFormat(normalizedDesc.format),
                           static_cast<GLsizei>(normalizedDesc.width),
                           static_cast<GLsizei>(normalizedDesc.height));
    }

    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    m_textures.push_back(OpenGLTexture{
        .id = texture,
        .desc = normalizedDesc,
        .subresource_states = std::vector<ResourceState>(normalizedDesc.mip_levels * normalizedDesc.array_layers,
                                                         normalizedDesc.initial_state),
        .is_swapchain_backbuffer = false,
    });
    return makeHandle<TextureHandle>(m_textures.size() - 1);
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
        .subresource_states =
            std::vector<ResourceState>{isDepthFormat(format) ? ResourceState::Undefined : ResourceState::Present},
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
