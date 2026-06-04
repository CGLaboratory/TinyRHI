#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "common/upload_helpers.h"
#include "common/win32_window.h"
#include "stb_image.h"
#include "TinyRHI/backend_factory.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "tiny_gltf.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <limits>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace lunalite::rhi;

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr uint32_t kEnvironmentSize = 512;
constexpr uint32_t kIrradianceSize = 32;
constexpr uint32_t kPrefilterSize = 128;
constexpr uint32_t kMinSampledPrefilterSize = 8;
constexpr uint32_t kBrdfLutSize = 512;
constexpr uint32_t kComputeGroupSize = 8;

struct Vec2 {
    float x{0.0f};
    float y{0.0f};
};

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct Vec4 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{0.0f};
};

struct Mat4 {
    float m[16]{};
};

struct Bounds {
    Vec3 min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 max{
        -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
};

struct Vertex {
    float position[3]{};
    float normal[3]{};
    float tangent[4]{};
    float uv[2]{};
};

struct MaterialFactors {
    Vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 properties{1.0f, 1.0f, 1.0f, 1.0f}; // metallic, roughness, normal scale, occlusion strength
};

struct GeometryPushConstants {
    Mat4 model{};
    Mat4 view_projection{};
    Vec4 base_color{};
    Vec4 properties{};
};

struct LightingPushConstants {
    Mat4 inverse_view_projection{};
    Vec4 camera_exposure{};
    Vec4 light_direction_intensity{};
    Vec4 light_color_ibl{};
    Vec4 params{};
};

struct PrimitiveResource {
    BufferHandle vertex_buffer{};
    BufferHandle index_buffer{};
    uint32_t index_count{0};
    Mat4 model{};
    int material_index{0};
};

struct GpuTexture {
    TextureHandle texture{};
    TextureViewHandle view{};
    TextureFormat format{TextureFormat::RGBA8_UNorm};
    uint32_t mip_levels{1};
};

struct MaterialResource {
    BindGroupHandle bind_group{};
    MaterialFactors factors{};
};

struct GpuModel {
    std::vector<PrimitiveResource> primitives;
    std::vector<MaterialResource> materials;
    std::vector<GpuTexture> textures;
    std::vector<GpuTexture> defaults;
    Bounds bounds{};
};

struct IBLResources {
    GpuTexture environment;
    GpuTexture irradiance;
    GpuTexture prefiltered;
    GpuTexture brdf_lut;
    SamplerHandle sampler{};
    uint32_t prefilter_mip_levels{1};
};

struct GBufferResources {
    uint32_t width{0};
    uint32_t height{0};
    GpuTexture position_roughness;
    GpuTexture normal_ao;
    GpuTexture albedo_metallic;
    GpuTexture depth;
    BindGroupHandle lighting_bind_group{};
};

struct FreeCamera {
    Vec3 position{};
    float yaw{0.0f};
    float pitch{0.0f};
};

Vec3 operator+(Vec3 lhs, Vec3 rhs)
{
    return Vec3{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 operator-(Vec3 lhs, Vec3 rhs)
{
    return Vec3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 operator*(Vec3 lhs, float rhs)
{
    return Vec3{lhs.x * rhs, lhs.y * rhs, lhs.z * rhs};
}

Vec3 operator/(Vec3 lhs, float rhs)
{
    return Vec3{lhs.x / rhs, lhs.y / rhs, lhs.z / rhs};
}

float dot(Vec3 lhs, Vec3 rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(Vec3 lhs, Vec3 rhs)
{
    return Vec3{
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

float length(Vec3 value)
{
    return std::sqrt(dot(value, value));
}

Vec3 normalize(Vec3 value)
{
    const float len = length(value);
    if (len <= 0.0f) {
        return Vec3{0.0f, 0.0f, 1.0f};
    }
    return value / len;
}

void includeBounds(Bounds& bounds, Vec3 value)
{
    bounds.min.x = std::min(bounds.min.x, value.x);
    bounds.min.y = std::min(bounds.min.y, value.y);
    bounds.min.z = std::min(bounds.min.z, value.z);
    bounds.max.x = std::max(bounds.max.x, value.x);
    bounds.max.y = std::max(bounds.max.y, value.y);
    bounds.max.z = std::max(bounds.max.z, value.z);
}

Mat4 identity()
{
    Mat4 result{};
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

Mat4 multiply(const Mat4& lhs, const Mat4& rhs)
{
    Mat4 result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0f;
            for (int i = 0; i < 4; ++i) {
                value += lhs.m[i * 4 + row] * rhs.m[column * 4 + i];
            }
            result.m[column * 4 + row] = value;
        }
    }
    return result;
}

Vec3 transformPoint(const Mat4& matrix, Vec3 value)
{
    const float x = matrix.m[0] * value.x + matrix.m[4] * value.y + matrix.m[8] * value.z + matrix.m[12];
    const float y = matrix.m[1] * value.x + matrix.m[5] * value.y + matrix.m[9] * value.z + matrix.m[13];
    const float z = matrix.m[2] * value.x + matrix.m[6] * value.y + matrix.m[10] * value.z + matrix.m[14];
    const float w = matrix.m[3] * value.x + matrix.m[7] * value.y + matrix.m[11] * value.z + matrix.m[15];
    if (std::abs(w) > 0.00001f) {
        return Vec3{x / w, y / w, z / w};
    }
    return Vec3{x, y, z};
}

Mat4 translationMatrix(Vec3 value)
{
    Mat4 result = identity();
    result.m[12] = value.x;
    result.m[13] = value.y;
    result.m[14] = value.z;
    return result;
}

Mat4 scaleMatrix(Vec3 value)
{
    Mat4 result = identity();
    result.m[0] = value.x;
    result.m[5] = value.y;
    result.m[10] = value.z;
    return result;
}

Mat4 rotationMatrixFromQuaternion(Vec4 q)
{
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len <= 0.0f) {
        return identity();
    }
    q.x /= len;
    q.y /= len;
    q.z /= len;
    q.w /= len;

    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;

    Mat4 result = identity();
    result.m[0] = 1.0f - 2.0f * (yy + zz);
    result.m[1] = 2.0f * (xy + wz);
    result.m[2] = 2.0f * (xz - wy);
    result.m[4] = 2.0f * (xy - wz);
    result.m[5] = 1.0f - 2.0f * (xx + zz);
    result.m[6] = 2.0f * (yz + wx);
    result.m[8] = 2.0f * (xz + wy);
    result.m[9] = 2.0f * (yz - wx);
    result.m[10] = 1.0f - 2.0f * (xx + yy);
    return result;
}

Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane)
{
    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4 result{};
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    return result;
}

Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up)
{
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    Mat4 result = identity();
    result.m[0] = s.x;
    result.m[1] = u.x;
    result.m[2] = -f.x;
    result.m[4] = s.y;
    result.m[5] = u.y;
    result.m[6] = -f.y;
    result.m[8] = s.z;
    result.m[9] = u.z;
    result.m[10] = -f.z;
    result.m[12] = -dot(s, eye);
    result.m[13] = -dot(u, eye);
    result.m[14] = dot(f, eye);
    return result;
}

Vec3 forwardFromYawPitch(float yaw, float pitch)
{
    const float cosPitch = std::cos(pitch);
    return normalize(Vec3{
        std::sin(yaw) * cosPitch,
        std::sin(pitch),
        std::cos(yaw) * cosPitch,
    });
}

POINT clientCenterToScreen(HWND hwnd)
{
    RECT rect{};
    GetClientRect(hwnd, &rect);
    POINT center{
        (rect.left + rect.right) / 2,
        (rect.top + rect.bottom) / 2,
    };
    ClientToScreen(hwnd, &center);
    return center;
}

bool invertMatrix(const Mat4& input, Mat4& output)
{
    const float* m = input.m;
    float inv[16]{};

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];

    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::abs(det) <= 0.0000001f) {
        output = identity();
        return false;
    }

    const float invDet = 1.0f / det;
    for (int i = 0; i < 16; ++i) {
        output.m[i] = inv[i] * invDet;
    }
    return true;
}

uint32_t calculateMipLevels(uint32_t width, uint32_t height)
{
    uint32_t levels = 1;
    while (width > 1 || height > 1) {
        width = width > 1 ? width / 2 : 1;
        height = height > 1 ? height / 2 : 1;
        ++levels;
    }
    return levels;
}

uint32_t mipDimension(uint32_t base, uint32_t mip)
{
    uint32_t value = base;
    for (uint32_t i = 0; i < mip && value > 1; ++i) {
        value /= 2;
    }
    return std::max(1u, value);
}

uint32_t maxSampledPrefilterMip(uint32_t mipLevels)
{
    uint32_t mip = 0;
    while (mip + 1 < mipLevels && mipDimension(kPrefilterSize, mip + 1) >= kMinSampledPrefilterSize) {
        ++mip;
    }
    return mip;
}

Vec3 cubeDirection(uint32_t face, uint32_t x, uint32_t y, uint32_t size)
{
    const float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size)) - 1.0f;
    const float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size)) - 1.0f;

    switch (face) {
        case 0:
            return normalize(Vec3{1.0f, -v, -u});
        case 1:
            return normalize(Vec3{-1.0f, -v, u});
        case 2:
            return normalize(Vec3{u, 1.0f, v});
        case 3:
            return normalize(Vec3{u, -1.0f, -v});
        case 4:
            return normalize(Vec3{u, -v, 1.0f});
        case 5:
            return normalize(Vec3{-u, -v, -1.0f});
    }

    return Vec3{0.0f, 0.0f, 1.0f};
}

void sampleEquirectangular(const float* hdr, int width, int height, Vec3 direction, float* outRgba)
{
    const float longitude = std::atan2(direction.z, direction.x);
    const float latitude = std::acos(std::clamp(direction.y, -1.0f, 1.0f));
    const float u = longitude / (2.0f * kPi) + 0.5f;
    const float v = latitude / kPi;

    const float px = u * static_cast<float>(width - 1);
    const float py = v * static_cast<float>(height - 1);
    const int x0 = std::clamp(static_cast<int>(std::floor(px)), 0, width - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(py)), 0, height - 1);
    const int x1 = (x0 + 1) % width;
    const int y1 = std::clamp(y0 + 1, 0, height - 1);
    const float tx = px - static_cast<float>(x0);
    const float ty = py - static_cast<float>(y0);

    const auto texel = [&](int x, int y) {
        return hdr + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
    };
    const float* c00 = texel(x0, y0);
    const float* c10 = texel(x1, y0);
    const float* c01 = texel(x0, y1);
    const float* c11 = texel(x1, y1);

    for (int channel = 0; channel < 3; ++channel) {
        const float top = c00[channel] * (1.0f - tx) + c10[channel] * tx;
        const float bottom = c01[channel] * (1.0f - tx) + c11[channel] * tx;
        outRgba[channel] = top * (1.0f - ty) + bottom * ty;
    }
    outRgba[3] = 1.0f;
}

std::vector<float> makeCubemapFace(const float* hdr, int width, int height, uint32_t face, uint32_t size)
{
    std::vector<float> pixels(static_cast<size_t>(size) * static_cast<size_t>(size) * 4);
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            sampleEquirectangular(hdr,
                                  width,
                                  height,
                                  cubeDirection(face, x, y, size),
                                  pixels.data() + (static_cast<size_t>(y) * static_cast<size_t>(size) + x) * 4);
        }
    }
    return pixels;
}

std::filesystem::path findFile(int argc, char** argv, const std::vector<std::filesystem::path>& candidates)
{
    const std::filesystem::path exeDir =
        argc > 0 ? std::filesystem::path(argv[0]).parent_path() : std::filesystem::path{};
    for (const auto& candidate : candidates) {
        const std::filesystem::path path = candidate.is_absolute() ? candidate : exeDir / candidate;
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return candidates.empty() ? std::filesystem::path{} : candidates.front();
}

Mat4 nodeLocalMatrix(const tinygltf::Node& node)
{
    if (node.matrix.size() == 16) {
        Mat4 result{};
        for (size_t i = 0; i < 16; ++i) {
            result.m[i] = static_cast<float>(node.matrix[i]);
        }
        return result;
    }

    Vec3 translation{0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    Vec4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    if (node.translation.size() == 3) {
        translation = Vec3{static_cast<float>(node.translation[0]),
                           static_cast<float>(node.translation[1]),
                           static_cast<float>(node.translation[2])};
    }
    if (node.scale.size() == 3) {
        scale = Vec3{
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2]),
        };
    }
    if (node.rotation.size() == 4) {
        rotation = Vec4{static_cast<float>(node.rotation[0]),
                        static_cast<float>(node.rotation[1]),
                        static_cast<float>(node.rotation[2]),
                        static_cast<float>(node.rotation[3])};
    }

    return multiply(translationMatrix(translation),
                    multiply(rotationMatrixFromQuaternion(rotation), scaleMatrix(scale)));
}

const unsigned char* accessorBase(const tinygltf::Model& model, const tinygltf::Accessor& accessor, int& stride)
{
    if (accessor.bufferView < 0 || static_cast<size_t>(accessor.bufferView) >= model.bufferViews.size()) {
        return nullptr;
    }
    const tinygltf::BufferView& view = model.bufferViews[static_cast<size_t>(accessor.bufferView)];
    if (view.buffer < 0 || static_cast<size_t>(view.buffer) >= model.buffers.size()) {
        return nullptr;
    }
    const tinygltf::Buffer& buffer = model.buffers[static_cast<size_t>(view.buffer)];
    stride = accessor.ByteStride(view);
    if (stride <= 0) {
        return nullptr;
    }
    const size_t offset = view.byteOffset + accessor.byteOffset;
    if (offset >= buffer.data.size()) {
        return nullptr;
    }
    return buffer.data.data() + offset;
}

Vec2 readVec2(const tinygltf::Model& model, int accessorIndex, size_t element)
{
    if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= model.accessors.size()) {
        return Vec2{};
    }
    const tinygltf::Accessor& accessor = model.accessors[static_cast<size_t>(accessorIndex)];
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC2 ||
        element >= accessor.count) {
        return Vec2{};
    }
    int stride = 0;
    const unsigned char* base = accessorBase(model, accessor, stride);
    if (base == nullptr) {
        return Vec2{};
    }
    const float* value = reinterpret_cast<const float*>(base + element * static_cast<size_t>(stride));
    return Vec2{value[0], value[1]};
}

Vec3 readVec3(const tinygltf::Model& model, int accessorIndex, size_t element, Vec3 fallback = Vec3{})
{
    if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= model.accessors.size()) {
        return fallback;
    }
    const tinygltf::Accessor& accessor = model.accessors[static_cast<size_t>(accessorIndex)];
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC3 ||
        element >= accessor.count) {
        return fallback;
    }
    int stride = 0;
    const unsigned char* base = accessorBase(model, accessor, stride);
    if (base == nullptr) {
        return fallback;
    }
    const float* value = reinterpret_cast<const float*>(base + element * static_cast<size_t>(stride));
    return Vec3{value[0], value[1], value[2]};
}

Vec4 readVec4(const tinygltf::Model& model, int accessorIndex, size_t element, Vec4 fallback = Vec4{})
{
    if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= model.accessors.size()) {
        return fallback;
    }
    const tinygltf::Accessor& accessor = model.accessors[static_cast<size_t>(accessorIndex)];
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC4 ||
        element >= accessor.count) {
        return fallback;
    }
    int stride = 0;
    const unsigned char* base = accessorBase(model, accessor, stride);
    if (base == nullptr) {
        return fallback;
    }
    const float* value = reinterpret_cast<const float*>(base + element * static_cast<size_t>(stride));
    return Vec4{value[0], value[1], value[2], value[3]};
}

std::vector<uint32_t> readIndices(const tinygltf::Model& model, int accessorIndex, size_t vertexCount)
{
    std::vector<uint32_t> indices;
    if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= model.accessors.size()) {
        indices.resize(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i) {
            indices[i] = static_cast<uint32_t>(i);
        }
        return indices;
    }

    const tinygltf::Accessor& accessor = model.accessors[static_cast<size_t>(accessorIndex)];
    if (accessor.type != TINYGLTF_TYPE_SCALAR) {
        return indices;
    }

    int stride = 0;
    const unsigned char* base = accessorBase(model, accessor, stride);
    if (base == nullptr) {
        return indices;
    }

    indices.resize(accessor.count);
    for (size_t i = 0; i < accessor.count; ++i) {
        const unsigned char* source = base + i * static_cast<size_t>(stride);
        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                indices[i] = *source;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                indices[i] = *reinterpret_cast<const uint16_t*>(source);
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                indices[i] = *reinterpret_cast<const uint32_t*>(source);
                break;
            default:
                indices.clear();
                return indices;
        }
    }
    return indices;
}

void computeTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    std::vector<Vec3> tangents(vertices.size());
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const uint32_t i0 = indices[i + 0];
        const uint32_t i1 = indices[i + 1];
        const uint32_t i2 = indices[i + 2];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            continue;
        }

        const Vec3 p0{vertices[i0].position[0], vertices[i0].position[1], vertices[i0].position[2]};
        const Vec3 p1{vertices[i1].position[0], vertices[i1].position[1], vertices[i1].position[2]};
        const Vec3 p2{vertices[i2].position[0], vertices[i2].position[1], vertices[i2].position[2]};
        const Vec2 uv0{vertices[i0].uv[0], vertices[i0].uv[1]};
        const Vec2 uv1{vertices[i1].uv[0], vertices[i1].uv[1]};
        const Vec2 uv2{vertices[i2].uv[0], vertices[i2].uv[1]};

        const Vec3 e1 = p1 - p0;
        const Vec3 e2 = p2 - p0;
        const float du1 = uv1.x - uv0.x;
        const float dv1 = uv1.y - uv0.y;
        const float du2 = uv2.x - uv0.x;
        const float dv2 = uv2.y - uv0.y;
        const float determinant = du1 * dv2 - du2 * dv1;
        if (std::abs(determinant) <= 0.000001f) {
            continue;
        }

        const float r = 1.0f / determinant;
        const Vec3 tangent = (e1 * dv2 - e2 * dv1) * r;
        tangents[i0] = tangents[i0] + tangent;
        tangents[i1] = tangents[i1] + tangent;
        tangents[i2] = tangents[i2] + tangent;
    }

    for (size_t i = 0; i < vertices.size(); ++i) {
        const Vec3 normal{vertices[i].normal[0], vertices[i].normal[1], vertices[i].normal[2]};
        Vec3 tangent = tangents[i];
        tangent = normalize(tangent - normal * dot(normal, tangent));
        vertices[i].tangent[0] = tangent.x;
        vertices[i].tangent[1] = tangent.y;
        vertices[i].tangent[2] = tangent.z;
        vertices[i].tangent[3] = 1.0f;
    }
}

TextureFormat textureFormatForImage(bool srgb)
{
    return srgb ? TextureFormat::RGBA8_SRGB : TextureFormat::RGBA8_UNorm;
}

GpuTexture createTextureFromPixels(Device& device,
                                   CommandListHandle commandList,
                                   const unsigned char* pixels,
                                   uint32_t width,
                                   uint32_t height,
                                   bool srgb)
{
    GpuTexture result{};
    if (pixels == nullptr || width == 0 || height == 0) {
        return result;
    }

    result.format = textureFormatForImage(srgb);
    result.mip_levels = calculateMipLevels(width, height);
    result.texture = device.createTexture(TextureDesc{
        .width = width,
        .height = height,
        .format = result.format,
        .usage = TextureUsage::Sampled | TextureUsage::CopySrc | TextureUsage::CopyDst,
        .mip_levels = result.mip_levels,
        .initial_state = ResourceState::CopyDst,
    });
    result.view = device.createTextureView(TextureViewDesc{
        .texture = result.texture,
        .format = result.format,
        .aspect = TextureAspect::Color,
        .mip_level_count = result.mip_levels,
    });

    const size_t rowPitch = static_cast<size_t>(width) * 4;
    const bool uploaded =
        tinyrhi_examples::uploadTextureData(device,
                                            commandList,
                                            result.texture,
                                            tinyrhi_examples::TextureUploadData{
                                                .data = pixels,
                                                .size = rowPitch * height,
                                                .row_pitch = rowPitch,
                                                .width = width,
                                                .height = height,
                                            }) &&
        tinyrhi_examples::transitionTextureToShaderRead(device, commandList, result.texture, result.mip_levels > 1);

    if (!uploaded) {
        device.destroyTextureView(result.view);
        device.destroyTexture(result.texture);
        result = {};
    }
    return result;
}

GpuTexture
    createSolidTexture(Device& device, CommandListHandle commandList, std::array<unsigned char, 4> color, bool srgb)
{
    return createTextureFromPixels(device, commandList, color.data(), 1, 1, srgb);
}

GpuTexture loadImageTexture(Device& device, CommandListHandle commandList, const std::filesystem::path& path, bool srgb)
{
    int width = 0;
    int height = 0;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, nullptr, STBI_rgb_alpha);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        std::printf("Failed to load image %s: %s\n", path.string().c_str(), stbi_failure_reason());
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return {};
    }

    GpuTexture result = createTextureFromPixels(
        device, commandList, pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), srgb);
    stbi_image_free(pixels);
    return result;
}

int gltfTextureToGpuIndex(const tinygltf::Model& model, int textureIndex)
{
    if (textureIndex < 0 || static_cast<size_t>(textureIndex) >= model.textures.size()) {
        return -1;
    }
    const int source = model.textures[static_cast<size_t>(textureIndex)].source;
    if (source < 0 || static_cast<size_t>(source) >= model.images.size()) {
        return -1;
    }
    return source;
}

BindGroupHandle createMaterialBindGroup(Device& device,
                                        BindGroupLayoutHandle layout,
                                        const GpuTexture& baseColor,
                                        const GpuTexture& metallicRoughness,
                                        const GpuTexture& normal,
                                        const GpuTexture& occlusion,
                                        SamplerHandle sampler)
{
    BindGroupDesc desc{};
    desc.layout = layout;
    desc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .texture_view = baseColor.view,
        .sampler = sampler,
    });
    desc.entries.push_back(BindGroupEntry{
        .binding = 1,
        .type = BindingType::CombinedImageSampler,
        .texture_view = metallicRoughness.view,
        .sampler = sampler,
    });
    desc.entries.push_back(BindGroupEntry{
        .binding = 2,
        .type = BindingType::CombinedImageSampler,
        .texture_view = normal.view,
        .sampler = sampler,
    });
    desc.entries.push_back(BindGroupEntry{
        .binding = 3,
        .type = BindingType::CombinedImageSampler,
        .texture_view = occlusion.view,
        .sampler = sampler,
    });
    return device.createBindGroup(desc);
}

bool loadGltfModel(Device& device,
                   CommandListHandle commandList,
                   const std::filesystem::path& path,
                   BindGroupLayoutHandle materialLayout,
                   SamplerHandle materialSampler,
                   GpuModel& outModel)
{
    tinygltf::TinyGLTF loader;
    loader.SetImagesAsIs(true);

    tinygltf::Model model;
    std::string error;
    std::string warning;
    const bool loaded = path.extension() == ".glb" ? loader.LoadBinaryFromFile(&model, &error, &warning, path.string())
                                                   : loader.LoadASCIIFromFile(&model, &error, &warning, path.string());
    if (!warning.empty()) {
        std::printf("tinygltf warning: %s\n", warning.c_str());
    }
    if (!loaded) {
        std::printf("Failed to load glTF %s: %s\n", path.string().c_str(), error.c_str());
        return false;
    }

    const std::filesystem::path baseDir = path.parent_path();
    outModel.defaults.push_back(createSolidTexture(device, commandList, {255, 255, 255, 255}, true));
    outModel.defaults.push_back(createSolidTexture(device, commandList, {255, 255, 255, 255}, false));
    outModel.defaults.push_back(createSolidTexture(device, commandList, {128, 128, 255, 255}, false));
    const GpuTexture& defaultBaseColor = outModel.defaults[0];
    const GpuTexture& defaultLinear = outModel.defaults[1];
    const GpuTexture& defaultNormal = outModel.defaults[2];

    outModel.textures.resize(model.images.size());
    for (size_t imageIndex = 0; imageIndex < model.images.size(); ++imageIndex) {
        const tinygltf::Image& image = model.images[imageIndex];
        if (image.uri.empty()) {
            continue;
        }
        const std::filesystem::path imagePath = baseDir / std::filesystem::path(image.uri);
        outModel.textures[imageIndex] = loadImageTexture(device, commandList, imagePath, false);
    }

    outModel.materials.reserve(std::max<size_t>(1, model.materials.size()));
    for (size_t materialIndex = 0; materialIndex < std::max<size_t>(1, model.materials.size()); ++materialIndex) {
        const tinygltf::Material* material =
            materialIndex < model.materials.size() ? &model.materials[materialIndex] : nullptr;

        MaterialFactors factors{};
        int baseColorTexture = -1;
        int metallicRoughnessTexture = -1;
        int normalTexture = -1;
        int occlusionTexture = -1;

        if (material != nullptr) {
            const auto& pbr = material->pbrMetallicRoughness;
            if (pbr.baseColorFactor.size() >= 4) {
                factors.base_color = Vec4{
                    static_cast<float>(pbr.baseColorFactor[0]),
                    static_cast<float>(pbr.baseColorFactor[1]),
                    static_cast<float>(pbr.baseColorFactor[2]),
                    static_cast<float>(pbr.baseColorFactor[3]),
                };
            }
            factors.properties.x = static_cast<float>(pbr.metallicFactor);
            factors.properties.y = static_cast<float>(pbr.roughnessFactor);
            factors.properties.z = static_cast<float>(material->normalTexture.scale);
            factors.properties.w = static_cast<float>(material->occlusionTexture.strength);

            baseColorTexture = gltfTextureToGpuIndex(model, pbr.baseColorTexture.index);
            metallicRoughnessTexture = gltfTextureToGpuIndex(model, pbr.metallicRoughnessTexture.index);
            normalTexture = gltfTextureToGpuIndex(model, material->normalTexture.index);
            occlusionTexture = gltfTextureToGpuIndex(model, material->occlusionTexture.index);
        }

        const auto textureOrDefault = [&](int index, const GpuTexture& fallback) -> const GpuTexture& {
            if (index >= 0 && static_cast<size_t>(index) < outModel.textures.size() && outModel.textures[index].view) {
                return outModel.textures[index];
            }
            return fallback;
        };

        const GpuTexture& baseColor = textureOrDefault(baseColorTexture, defaultBaseColor);
        const GpuTexture& metallicRoughness = textureOrDefault(metallicRoughnessTexture, defaultLinear);
        const GpuTexture& normal = textureOrDefault(normalTexture, defaultNormal);
        const GpuTexture& occlusion = textureOrDefault(occlusionTexture, defaultLinear);

        MaterialResource materialResource{};
        materialResource.factors = factors;
        materialResource.bind_group = createMaterialBindGroup(
            device, materialLayout, baseColor, metallicRoughness, normal, occlusion, materialSampler);
        outModel.materials.push_back(materialResource);
    }

    const auto makePrimitive = [&](const tinygltf::Primitive& primitive,
                                   const Mat4& nodeTransform,
                                   std::vector<Vertex>& vertices,
                                   std::vector<uint32_t>& indices) {
        if (primitive.mode != -1 && primitive.mode != TINYGLTF_MODE_TRIANGLES) {
            return false;
        }
        const auto positionIt = primitive.attributes.find("POSITION");
        if (positionIt == primitive.attributes.end() || positionIt->second < 0 ||
            static_cast<size_t>(positionIt->second) >= model.accessors.size()) {
            return false;
        }

        const int positionAccessor = positionIt->second;
        const int normalAccessor = primitive.attributes.count("NORMAL") != 0 ? primitive.attributes.at("NORMAL") : -1;
        const int tangentAccessor =
            primitive.attributes.count("TANGENT") != 0 ? primitive.attributes.at("TANGENT") : -1;
        const int uvAccessor =
            primitive.attributes.count("TEXCOORD_0") != 0 ? primitive.attributes.at("TEXCOORD_0") : -1;

        const size_t vertexCount = model.accessors[static_cast<size_t>(positionAccessor)].count;
        vertices.resize(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i) {
            const Vec3 position = readVec3(model, positionAccessor, i);
            const Vec3 normal = normalize(readVec3(model, normalAccessor, i, Vec3{0.0f, 1.0f, 0.0f}));
            const Vec4 tangent = readVec4(model, tangentAccessor, i, Vec4{0.0f, 0.0f, 0.0f, 1.0f});
            const Vec2 uv = readVec2(model, uvAccessor, i);

            vertices[i].position[0] = position.x;
            vertices[i].position[1] = position.y;
            vertices[i].position[2] = position.z;
            vertices[i].normal[0] = normal.x;
            vertices[i].normal[1] = normal.y;
            vertices[i].normal[2] = normal.z;
            vertices[i].tangent[0] = tangent.x;
            vertices[i].tangent[1] = tangent.y;
            vertices[i].tangent[2] = tangent.z;
            vertices[i].tangent[3] = tangent.w == 0.0f ? 1.0f : tangent.w;
            vertices[i].uv[0] = uv.x;
            vertices[i].uv[1] = uv.y;

            includeBounds(outModel.bounds, transformPoint(nodeTransform, position));
        }

        indices = readIndices(model, primitive.indices, vertexCount);
        if (indices.empty()) {
            return false;
        }
        if (tangentAccessor < 0) {
            computeTangents(vertices, indices);
        }
        return true;
    };

    const auto addMesh = [&](const tinygltf::Mesh& mesh, const Mat4& nodeTransform) {
        for (const tinygltf::Primitive& primitive : mesh.primitives) {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            if (!makePrimitive(primitive, nodeTransform, vertices, indices)) {
                continue;
            }

            PrimitiveResource resource{};
            resource.vertex_buffer =
                tinyrhi_examples::createStaticBuffer(device,
                                                     commandList,
                                                     BufferDesc{
                                                         .size = vertices.size() * sizeof(Vertex),
                                                         .usage = BufferUsage::Vertex,
                                                         .initial_state = ResourceState::VertexBuffer,
                                                     },
                                                     vertices.data());
            resource.index_buffer =
                tinyrhi_examples::createStaticBuffer(device,
                                                     commandList,
                                                     BufferDesc{
                                                         .size = indices.size() * sizeof(uint32_t),
                                                         .usage = BufferUsage::Index,
                                                         .initial_state = ResourceState::IndexBuffer,
                                                     },
                                                     indices.data());
            resource.index_count = static_cast<uint32_t>(indices.size());
            resource.model = nodeTransform;
            resource.material_index =
                primitive.material >= 0 && static_cast<size_t>(primitive.material) < outModel.materials.size()
                    ? primitive.material
                    : 0;
            if (resource.vertex_buffer && resource.index_buffer && resource.index_count > 0) {
                outModel.primitives.push_back(resource);
            }
        }
    };

    const auto traverseNode = [&](const auto& self, int nodeIndex, const Mat4& parentTransform) -> void {
        if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= model.nodes.size()) {
            return;
        }
        const tinygltf::Node& node = model.nodes[static_cast<size_t>(nodeIndex)];
        const Mat4 transform = multiply(parentTransform, nodeLocalMatrix(node));
        if (node.mesh >= 0 && static_cast<size_t>(node.mesh) < model.meshes.size()) {
            addMesh(model.meshes[static_cast<size_t>(node.mesh)], transform);
        }
        for (const int child : node.children) {
            self(self, child, transform);
        }
    };

    if (!model.scenes.empty()) {
        const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
        if (sceneIndex >= 0 && static_cast<size_t>(sceneIndex) < model.scenes.size()) {
            for (const int node : model.scenes[static_cast<size_t>(sceneIndex)].nodes) {
                traverseNode(traverseNode, node, identity());
            }
        }
    } else {
        for (size_t node = 0; node < model.nodes.size(); ++node) {
            traverseNode(traverseNode, static_cast<int>(node), identity());
        }
    }

    return !outModel.primitives.empty();
}

GpuTexture createRenderTexture(Device& device,
                               uint32_t width,
                               uint32_t height,
                               TextureFormat format,
                               TextureUsage usage,
                               ResourceState initialState = ResourceState::Undefined)
{
    GpuTexture result{};
    result.format = format;
    result.mip_levels = 1;
    result.texture = device.createTexture(TextureDesc{
        .width = width,
        .height = height,
        .format = format,
        .usage = usage,
        .mip_levels = 1,
        .initial_state = initialState,
    });
    result.view = device.createTextureView(TextureViewDesc{
        .texture = result.texture,
        .format = format,
        .aspect = format == TextureFormat::Depth24Stencil8 ? TextureAspect::DepthStencil : TextureAspect::Color,
        .mip_level_count = 1,
    });
    return result;
}

void destroyTexture(Device& device, GpuTexture& texture)
{
    if (texture.view) {
        device.destroyTextureView(texture.view);
    }
    if (texture.texture) {
        device.destroyTexture(texture.texture);
    }
    texture = {};
}

void destroyGBuffer(Device& device, GBufferResources& gbuffer)
{
    if (gbuffer.lighting_bind_group) {
        device.destroyBindGroup(gbuffer.lighting_bind_group);
    }
    destroyTexture(device, gbuffer.position_roughness);
    destroyTexture(device, gbuffer.normal_ao);
    destroyTexture(device, gbuffer.albedo_metallic);
    destroyTexture(device, gbuffer.depth);
    gbuffer = {};
}

BindGroupHandle createLightingBindGroup(Device& device,
                                        BindGroupLayoutHandle layout,
                                        const GBufferResources& gbuffer,
                                        const IBLResources& ibl)
{
    BindGroupDesc desc{};
    desc.layout = layout;
    desc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .texture_view = gbuffer.position_roughness.view,
        .sampler = ibl.sampler,
    });
    desc.entries.push_back(BindGroupEntry{
        .binding = 1,
        .type = BindingType::CombinedImageSampler,
        .texture_view = gbuffer.normal_ao.view,
        .sampler = ibl.sampler,
    });
    desc.entries.push_back(BindGroupEntry{
        .binding = 2,
        .type = BindingType::CombinedImageSampler,
        .texture_view = gbuffer.albedo_metallic.view,
        .sampler = ibl.sampler,
    });
    desc.entries.push_back(BindGroupEntry{
        .binding = 3,
        .type = BindingType::CombinedImageSampler,
        .texture_view = ibl.irradiance.view,
        .sampler = ibl.sampler,
    });
    desc.entries.push_back(BindGroupEntry{
        .binding = 4,
        .type = BindingType::CombinedImageSampler,
        .texture_view = ibl.prefiltered.view,
        .sampler = ibl.sampler,
    });
    desc.entries.push_back(BindGroupEntry{
        .binding = 5,
        .type = BindingType::CombinedImageSampler,
        .texture_view = ibl.brdf_lut.view,
        .sampler = ibl.sampler,
    });
    return device.createBindGroup(desc);
}

bool ensureGBuffer(Device& device,
                   uint32_t width,
                   uint32_t height,
                   BindGroupLayoutHandle lightingLayout,
                   const IBLResources& ibl,
                   GBufferResources& gbuffer)
{
    if (width == 0 || height == 0) {
        return false;
    }
    if (gbuffer.width == width && gbuffer.height == height && gbuffer.lighting_bind_group) {
        return true;
    }

    destroyGBuffer(device, gbuffer);
    gbuffer.width = width;
    gbuffer.height = height;
    gbuffer.position_roughness = createRenderTexture(
        device, width, height, TextureFormat::RGBA16F, TextureUsage::RenderTarget | TextureUsage::Sampled);
    gbuffer.normal_ao = createRenderTexture(
        device, width, height, TextureFormat::RGBA16F, TextureUsage::RenderTarget | TextureUsage::Sampled);
    gbuffer.albedo_metallic = createRenderTexture(
        device, width, height, TextureFormat::RGBA16F, TextureUsage::RenderTarget | TextureUsage::Sampled);
    gbuffer.depth =
        createRenderTexture(device, width, height, TextureFormat::Depth24Stencil8, TextureUsage::DepthStencil);
    gbuffer.lighting_bind_group = createLightingBindGroup(device, lightingLayout, gbuffer, ibl);

    return gbuffer.position_roughness.view && gbuffer.normal_ao.view && gbuffer.albedo_metallic.view &&
           gbuffer.depth.view && gbuffer.lighting_bind_group;
}

constexpr const char* kGeometryVertexShader = R"GLSL(
#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inUV;

uniform vec4 uPushConstants[10];

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec4 vWorldTangent;
out vec2 vUV;

mat4 pcMat4(int offset)
{
    return mat4(
        uPushConstants[offset + 0],
        uPushConstants[offset + 1],
        uPushConstants[offset + 2],
        uPushConstants[offset + 3]);
}

void main()
{
    mat4 model = pcMat4(0);
    mat4 viewProjection = pcMat4(4);
    vec4 worldPosition = model * vec4(inPosition, 1.0);
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 normal = normalize(normalMatrix * inNormal);
    vec3 tangent = normalize(mat3(model) * inTangent.xyz);

    vWorldPosition = worldPosition.xyz;
    vWorldNormal = normal;
    vWorldTangent = vec4(tangent, inTangent.w);
    vUV = inUV;
    gl_Position = viewProjection * worldPosition;
}
)GLSL";

constexpr const char* kGeometryFragmentShader = R"GLSL(
#version 450 core
layout(binding = 0) uniform sampler2D uBaseColor;
layout(binding = 1) uniform sampler2D uMetallicRoughness;
layout(binding = 2) uniform sampler2D uNormal;
layout(binding = 3) uniform sampler2D uOcclusion;

uniform vec4 uPushConstants[10];

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec4 vWorldTangent;
in vec2 vUV;

layout(location = 0) out vec4 outPositionRoughness;
layout(location = 1) out vec4 outNormalAO;
layout(location = 2) out vec4 outAlbedoMetallic;

void main()
{
    vec4 baseColorFactor = uPushConstants[8];
    vec4 properties = uPushConstants[9];
    vec4 baseSample = texture(uBaseColor, vUV) * baseColorFactor;
    vec4 mrSample = texture(uMetallicRoughness, vUV);

    float metallic = clamp(mrSample.b * properties.x, 0.0, 1.0);
    float roughness = clamp(mrSample.g * properties.y, 0.045, 1.0);
    float ao = mix(1.0, texture(uOcclusion, vUV).r, clamp(properties.w, 0.0, 1.0));

    vec3 n = normalize(vWorldNormal);
    vec3 t = normalize(vWorldTangent.xyz - n * dot(n, vWorldTangent.xyz));
    vec3 b = normalize(cross(n, t) * vWorldTangent.w);
    vec3 tangentNormal = texture(uNormal, vUV).xyz * 2.0 - 1.0;
    tangentNormal.xy *= properties.z;
    vec3 normal = normalize(mat3(t, b, n) * tangentNormal);

    outPositionRoughness = vec4(vWorldPosition, roughness);
    outNormalAO = vec4(normal, ao);
    outAlbedoMetallic = vec4(baseSample.rgb, metallic);
}
)GLSL";

constexpr const char* kFullscreenVertexShader = R"GLSL(
#version 450 core
out vec2 vUV;

void main()
{
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0));
    vec2 position = positions[gl_VertexID];
    vUV = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)GLSL";

constexpr const char* kLightingFragmentShader = R"GLSL(
#version 450 core
layout(binding = 0) uniform sampler2D uPositionRoughness;
layout(binding = 1) uniform sampler2D uNormalAO;
layout(binding = 2) uniform sampler2D uAlbedoMetallic;
layout(binding = 3) uniform samplerCube uIrradiance;
layout(binding = 4) uniform samplerCube uPrefiltered;
layout(binding = 5) uniform sampler2D uBRDFLut;

uniform vec4 uPushConstants[8];

in vec2 vUV;
out vec4 outColor;

const float PI = 3.14159265359;

mat4 pcMat4(int offset)
{
    return mat4(
        uPushConstants[offset + 0],
        uPushConstants[offset + 1],
        uPushConstants[offset + 2],
        uPushConstants[offset + 3]);
}

float distributionGGX(float nDotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = (nDotH * nDotH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 0.000001);
}

float visibilitySmithGGXCorrelated(float nDotV, float nDotL, float roughness)
{
    float a = roughness * roughness;
    float gv = nDotL * sqrt(max(nDotV * (nDotV - nDotV * a) + a, 0.0));
    float gl = nDotV * sqrt(max(nDotL * (nDotL - nDotL * a) + a, 0.0));
    return 0.5 / max(gv + gl, 0.000001);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 acesApprox(vec3 color)
{
    color *= 0.6;
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 backgroundDirection(vec2 uv, vec3 cameraPosition)
{
    mat4 inverseViewProjection = pcMat4(0);
    vec2 ndc = uv * 2.0 - 1.0;
    vec4 world = inverseViewProjection * vec4(ndc, 1.0, 1.0);
    world.xyz /= world.w;
    return normalize(world.xyz - cameraPosition);
}

void main()
{
    vec4 positionRoughness = texture(uPositionRoughness, vUV);
    vec4 normalAO = texture(uNormalAO, vUV);
    vec4 albedoMetallic = texture(uAlbedoMetallic, vUV);

    vec3 cameraPosition = uPushConstants[4].xyz;
    float exposure = uPushConstants[4].w;
    vec3 lightDirection = normalize(uPushConstants[5].xyz);
    float lightIntensity = uPushConstants[5].w;
    vec3 lightColor = uPushConstants[6].rgb;
    float iblIntensity = uPushConstants[6].w;
    float maxPrefilterMip = uPushConstants[7].x;

    vec3 normal = normalize(normalAO.xyz);
    if (length(normalAO.xyz) < 0.5) {
        vec3 sky = textureLod(uPrefiltered, backgroundDirection(vUV, cameraPosition), 0.0).rgb * iblIntensity;
        outColor = vec4(acesApprox(sky * exposure), 1.0);
        return;
    }

    vec3 worldPosition = positionRoughness.xyz;
    float roughness = clamp(positionRoughness.w, 0.045, 1.0);
    float ao = clamp(normalAO.w, 0.0, 1.0);
    vec3 albedo = max(albedoMetallic.rgb, vec3(0.0));
    float metallic = clamp(albedoMetallic.a, 0.0, 1.0);

    vec3 view = normalize(cameraPosition - worldPosition);
    vec3 light = normalize(-lightDirection);
    vec3 halfVector = normalize(view + light);
    vec3 reflection = reflect(-view, normal);

    float nDotV = max(dot(normal, view), 0.001);
    float nDotL = max(dot(normal, light), 0.0);
    float nDotH = max(dot(normal, halfVector), 0.0);
    float vDotH = max(dot(view, halfVector), 0.0);

    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 f = fresnelSchlick(vDotH, f0);
    float d = distributionGGX(nDotH, roughness);
    float v = visibilitySmithGGXCorrelated(nDotV, nDotL, roughness);
    vec3 specular = d * v * f;
    vec3 diffuse = (1.0 - f) * albedo / PI * (1.0 - metallic);
    vec3 direct = (diffuse + specular) * lightColor * lightIntensity * nDotL;

    vec3 iblF = fresnelSchlick(nDotV, f0);
    vec3 irradiance = texture(uIrradiance, normal).rgb;
    vec3 diffuseIBL = irradiance * albedo * (1.0 - metallic);
    float prefilterLod = clamp(roughness * maxPrefilterMip, 0.0, maxPrefilterMip);
    vec3 prefiltered = textureLod(uPrefiltered, reflection, prefilterLod).rgb;
    vec2 brdf = texture(uBRDFLut, vec2(nDotV, roughness)).rg;
    vec3 specularIBL = prefiltered * (iblF * brdf.x + brdf.y);
    vec3 indirect = (diffuseIBL + specularIBL) * iblIntensity * ao;

    outColor = vec4(acesApprox((direct + indirect) * exposure), 1.0);
}
)GLSL";

constexpr const char* kBRDFComputeShader = R"GLSL(
#version 450 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rg16f, binding = 0) writeonly uniform image2D uBRDFLut;

const float PI = 3.14159265359;
const uint SAMPLE_COUNT = 1024u;

float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint n)
{
    return vec2(float(i) / float(n), RadicalInverseVdC(i));
}

vec3 ImportanceSampleGGX(vec2 xi, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float GeometrySchlickGGX(float nDotV, float roughness)
{
    float a = roughness;
    float k = (a * a) * 0.5;
    return nDotV / max(nDotV * (1.0 - k) + k, 0.0001);
}

float GeometrySmith(float nDotV, float nDotL, float roughness)
{
    return GeometrySchlickGGX(nDotV, roughness) * GeometrySchlickGGX(nDotL, roughness);
}

vec2 IntegrateBRDF(float nDotV, float roughness)
{
    vec3 view = vec3(sqrt(max(1.0 - nDotV * nDotV, 0.0)), 0.0, nDotV);
    float scale = 0.0;
    float bias = 0.0;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 xi = Hammersley(i, SAMPLE_COUNT);
        vec3 h = ImportanceSampleGGX(xi, roughness);
        vec3 l = normalize(2.0 * dot(view, h) * h - view);
        float nDotL = max(l.z, 0.0);
        float nDotH = max(h.z, 0.0);
        float vDotH = max(dot(view, h), 0.0);
        if (nDotL > 0.0) {
            float geometry = GeometrySmith(nDotV, nDotL, roughness);
            float visibility = (geometry * vDotH) / max(nDotH * nDotV, 0.001);
            float fresnel = pow(1.0 - vDotH, 5.0);
            scale += (1.0 - fresnel) * visibility;
            bias += fresnel * visibility;
        }
    }
    return vec2(scale, bias) / float(SAMPLE_COUNT);
}

void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(uBRDFLut);
    if (pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }
    vec2 uv = (vec2(pixel) + vec2(0.5)) / vec2(size);
    vec2 integrated = IntegrateBRDF(max(uv.x, 0.001), max(uv.y, 0.001));
    imageStore(uBRDFLut, pixel, vec4(integrated, 0.0, 1.0));
}
)GLSL";

constexpr const char* kIrradianceComputeShader = R"GLSL(
#version 450 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(binding = 0) uniform samplerCube uEnvironment;
layout(rgba16f, binding = 1) writeonly uniform imageCube uIrradiance;

const float PI = 3.14159265359;

vec3 cubeDirection(uint face, vec2 uv)
{
    uv = uv * 2.0 - 1.0;
    if (face == 0u) return normalize(vec3(1.0, -uv.y, -uv.x));
    if (face == 1u) return normalize(vec3(-1.0, -uv.y, uv.x));
    if (face == 2u) return normalize(vec3(uv.x, 1.0, uv.y));
    if (face == 3u) return normalize(vec3(uv.x, -1.0, -uv.y));
    if (face == 4u) return normalize(vec3(uv.x, -uv.y, 1.0));
    return normalize(vec3(-uv.x, -uv.y, -1.0));
}

void main()
{
    ivec2 size = imageSize(uIrradiance);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    int face = int(gl_GlobalInvocationID.z);
    if (pixel.x >= size.x || pixel.y >= size.y || face >= 6) {
        return;
    }

    vec2 uv = (vec2(pixel) + vec2(0.5)) / vec2(size);
    vec3 normal = cubeDirection(uint(face), uv);
    vec3 up = abs(normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    vec3 irradiance = vec3(0.0);
    float sampleCount = 0.0;
    const float sampleDelta = 0.08;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
            irradiance += textureLod(uEnvironment, sampleVec, 0.0).rgb * cos(theta) * sin(theta);
            sampleCount += 1.0;
        }
    }
    irradiance = PI * irradiance / max(sampleCount, 1.0);
    imageStore(uIrradiance, ivec3(pixel, face), vec4(irradiance, 1.0));
}
)GLSL";

constexpr const char* kPrefilterComputeShader = R"GLSL(
#version 450 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(binding = 0) uniform samplerCube uEnvironment;
layout(rgba16f, binding = 1) writeonly uniform imageCube uPrefiltered;

uniform vec4 uPushConstants[1];

const float PI = 3.14159265359;
const uint SAMPLE_COUNT = 1024u;

float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint n)
{
    return vec2(float(i) / float(n), RadicalInverseVdC(i));
}

vec3 ImportanceSampleGGX(vec2 xi, vec3 n, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    vec3 h = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, n));
    vec3 bitangent = cross(n, tangent);
    return normalize(tangent * h.x + bitangent * h.y + n * h.z);
}

vec3 cubeDirection(uint face, vec2 uv)
{
    uv = uv * 2.0 - 1.0;
    if (face == 0u) return normalize(vec3(1.0, -uv.y, -uv.x));
    if (face == 1u) return normalize(vec3(-1.0, -uv.y, uv.x));
    if (face == 2u) return normalize(vec3(uv.x, 1.0, uv.y));
    if (face == 3u) return normalize(vec3(uv.x, -1.0, -uv.y));
    if (face == 4u) return normalize(vec3(uv.x, -uv.y, 1.0));
    return normalize(vec3(-uv.x, -uv.y, -1.0));
}

void main()
{
    ivec2 size = imageSize(uPrefiltered);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    int face = int(gl_GlobalInvocationID.z);
    if (pixel.x >= size.x || pixel.y >= size.y || face >= 6) {
        return;
    }

    float roughness = clamp(uPushConstants[0].x, 0.0, 1.0);
    vec2 uv = (vec2(pixel) + vec2(0.5)) / vec2(size);
    vec3 n = cubeDirection(uint(face), uv);
    vec3 r = n;
    vec3 v = r;

    float totalWeight = 0.0;
    vec3 prefiltered = vec3(0.0);
    float envResolution = float(textureSize(uEnvironment, 0).x);
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 xi = Hammersley(i, SAMPLE_COUNT);
        vec3 h = ImportanceSampleGGX(xi, n, roughness);
        vec3 l = normalize(2.0 * dot(v, h) * h - v);
        float nDotL = max(dot(n, l), 0.0);
        if (nDotL > 0.0) {
            float nDotH = max(dot(n, h), 0.0);
            float hDotV = max(dot(h, v), 0.0);
            float a = roughness * roughness;
            float a2 = a * a;
            float denom = nDotH * nDotH * (a2 - 1.0) + 1.0;
            float d = a2 / max(PI * denom * denom, 0.000001);
            float pdf = d * nDotH / max(4.0 * hDotV, 0.0001) + 0.0001;
            float saTexel = 4.0 * PI / (6.0 * envResolution * envResolution);
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf);
            float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);
            prefiltered += textureLod(uEnvironment, l, mipLevel).rgb * nDotL;
            totalWeight += nDotL;
        }
    }
    prefiltered = prefiltered / max(totalWeight, 0.0001);
    imageStore(uPrefiltered, ivec3(pixel, face), vec4(prefiltered, 1.0));
}
)GLSL";

bool createEnvironmentCubemap(Device& device,
                              CommandListHandle commandList,
                              const std::filesystem::path& hdrPath,
                              GpuTexture& environment)
{
    int width = 0;
    int height = 0;
    float* hdrPixels = stbi_loadf(hdrPath.string().c_str(), &width, &height, nullptr, 4);
    if (hdrPixels == nullptr || width <= 0 || height <= 0) {
        std::printf("Failed to load HDR %s: %s\n", hdrPath.string().c_str(), stbi_failure_reason());
        if (hdrPixels != nullptr) {
            stbi_image_free(hdrPixels);
        }
        return false;
    }

    environment.format = TextureFormat::RGBA32F;
    environment.mip_levels = calculateMipLevels(kEnvironmentSize, kEnvironmentSize);
    environment.texture = device.createTexture(TextureDesc{
        .width = kEnvironmentSize,
        .height = kEnvironmentSize,
        .dimension = TextureDimension::TextureCube,
        .format = environment.format,
        .usage = TextureUsage::Sampled | TextureUsage::CopySrc | TextureUsage::CopyDst,
        .mip_levels = environment.mip_levels,
        .array_layers = 6,
        .initial_state = ResourceState::CopyDst,
    });
    environment.view = device.createTextureView(TextureViewDesc{
        .texture = environment.texture,
        .view_dimension = TextureViewDimension::TextureCube,
        .format = environment.format,
        .aspect = TextureAspect::Color,
        .mip_level_count = environment.mip_levels,
        .array_layer_count = 6,
    });

    bool uploaded = environment.texture && environment.view;
    for (uint32_t face = 0; uploaded && face < 6; ++face) {
        const std::vector<float> facePixels = makeCubemapFace(hdrPixels, width, height, face, kEnvironmentSize);
        const size_t rowPitch = static_cast<size_t>(kEnvironmentSize) * 4 * sizeof(float);
        uploaded = tinyrhi_examples::uploadTextureData(device,
                                                       commandList,
                                                       environment.texture,
                                                       tinyrhi_examples::TextureUploadData{
                                                           .data = facePixels.data(),
                                                           .size = rowPitch * kEnvironmentSize,
                                                           .row_pitch = rowPitch,
                                                           .width = kEnvironmentSize,
                                                           .height = kEnvironmentSize,
                                                           .array_layer = face,
                                                       });
    }
    stbi_image_free(hdrPixels);

    uploaded =
        uploaded && tinyrhi_examples::transitionTextureToShaderRead(device, commandList, environment.texture, true);
    if (!uploaded) {
        destroyTexture(device, environment);
    }
    return uploaded;
}

TextureViewHandle createCubeMipView(Device& device, TextureHandle texture, TextureFormat format, uint32_t mipLevel)
{
    return device.createTextureView(TextureViewDesc{
        .texture = texture,
        .view_dimension = TextureViewDimension::TextureCube,
        .format = format,
        .aspect = TextureAspect::Color,
        .base_mip_level = mipLevel,
        .mip_level_count = 1,
        .base_array_layer = 0,
        .array_layer_count = 6,
    });
}

bool precomputeIBL(Device& device,
                   CommandListHandle commandList,
                   const std::filesystem::path& hdrPath,
                   IBLResources& ibl)
{
    ibl.sampler = device.createSampler(SamplerDesc{
        .min_filter = FilterMode::Linear,
        .mag_filter = FilterMode::Linear,
        .mip_filter = MipFilter::Linear,
        .address_u = AddressMode::ClampToEdge,
        .address_v = AddressMode::ClampToEdge,
        .address_w = AddressMode::ClampToEdge,
    });
    if (!ibl.sampler || !createEnvironmentCubemap(device, commandList, hdrPath, ibl.environment)) {
        return false;
    }

    ibl.irradiance.format = TextureFormat::RGBA16F;
    ibl.irradiance.mip_levels = 1;
    ibl.irradiance.texture = device.createTexture(TextureDesc{
        .width = kIrradianceSize,
        .height = kIrradianceSize,
        .dimension = TextureDimension::TextureCube,
        .format = ibl.irradiance.format,
        .usage = TextureUsage::Sampled | TextureUsage::Storage,
        .mip_levels = 1,
        .array_layers = 6,
    });
    ibl.irradiance.view = device.createTextureView(TextureViewDesc{
        .texture = ibl.irradiance.texture,
        .view_dimension = TextureViewDimension::TextureCube,
        .format = ibl.irradiance.format,
        .aspect = TextureAspect::Color,
        .mip_level_count = 1,
        .array_layer_count = 6,
    });

    ibl.prefilter_mip_levels = calculateMipLevels(kPrefilterSize, kPrefilterSize);
    ibl.prefiltered.format = TextureFormat::RGBA16F;
    ibl.prefiltered.mip_levels = ibl.prefilter_mip_levels;
    ibl.prefiltered.texture = device.createTexture(TextureDesc{
        .width = kPrefilterSize,
        .height = kPrefilterSize,
        .dimension = TextureDimension::TextureCube,
        .format = ibl.prefiltered.format,
        .usage = TextureUsage::Sampled | TextureUsage::Storage,
        .mip_levels = ibl.prefilter_mip_levels,
        .array_layers = 6,
    });
    ibl.prefiltered.view = device.createTextureView(TextureViewDesc{
        .texture = ibl.prefiltered.texture,
        .view_dimension = TextureViewDimension::TextureCube,
        .format = ibl.prefiltered.format,
        .aspect = TextureAspect::Color,
        .mip_level_count = ibl.prefilter_mip_levels,
        .array_layer_count = 6,
    });

    ibl.brdf_lut.format = TextureFormat::RG16F;
    ibl.brdf_lut.mip_levels = 1;
    ibl.brdf_lut.texture = device.createTexture(TextureDesc{
        .width = kBrdfLutSize,
        .height = kBrdfLutSize,
        .format = ibl.brdf_lut.format,
        .usage = TextureUsage::Sampled | TextureUsage::Storage,
    });
    ibl.brdf_lut.view = device.createTextureView(TextureViewDesc{
        .texture = ibl.brdf_lut.texture,
        .format = ibl.brdf_lut.format,
        .aspect = TextureAspect::Color,
        .mip_level_count = 1,
    });

    ShaderHandle brdfShader =
        device.createShader(ShaderDesc{.stage = ShaderStage::Compute, .source = kBRDFComputeShader});
    ShaderHandle irradianceShader =
        device.createShader(ShaderDesc{.stage = ShaderStage::Compute, .source = kIrradianceComputeShader});
    ShaderHandle prefilterShader =
        device.createShader(ShaderDesc{.stage = ShaderStage::Compute, .source = kPrefilterComputeShader});

    BindGroupLayoutDesc brdfLayoutDesc{};
    brdfLayoutDesc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::StorageTexture,
        .stages = shaderStageFlag(ShaderStage::Compute),
    });
    BindGroupLayoutHandle brdfLayout = device.createBindGroupLayout(brdfLayoutDesc);

    BindGroupLayoutDesc cubeLayoutDesc{};
    cubeLayoutDesc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .stages = shaderStageFlag(ShaderStage::Compute),
    });
    cubeLayoutDesc.entries.push_back(BindGroupLayoutEntry{
        .binding = 1,
        .type = BindingType::StorageTexture,
        .stages = shaderStageFlag(ShaderStage::Compute),
    });
    BindGroupLayoutHandle cubeLayout = device.createBindGroupLayout(cubeLayoutDesc);

    PipelineLayoutDesc brdfPipelineLayoutDesc{};
    brdfPipelineLayoutDesc.bind_group_layouts.push_back(brdfLayout);
    PipelineLayoutHandle brdfPipelineLayout = device.createPipelineLayout(brdfPipelineLayoutDesc);

    PipelineLayoutDesc cubePipelineLayoutDesc{};
    cubePipelineLayoutDesc.bind_group_layouts.push_back(cubeLayout);
    PipelineLayoutHandle cubePipelineLayout = device.createPipelineLayout(cubePipelineLayoutDesc);

    PipelineLayoutDesc prefilterPipelineLayoutDesc{};
    prefilterPipelineLayoutDesc.bind_group_layouts.push_back(cubeLayout);
    prefilterPipelineLayoutDesc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Compute),
        .offset = 0,
        .size = sizeof(Vec4),
    });
    PipelineLayoutHandle prefilterPipelineLayout = device.createPipelineLayout(prefilterPipelineLayoutDesc);

    PipelineHandle brdfPipeline = device.createComputePipeline(ComputePipelineDesc{
        .layout = brdfPipelineLayout,
        .compute_shader = brdfShader,
    });
    PipelineHandle irradiancePipeline = device.createComputePipeline(ComputePipelineDesc{
        .layout = cubePipelineLayout,
        .compute_shader = irradianceShader,
    });
    PipelineHandle prefilterPipeline = device.createComputePipeline(ComputePipelineDesc{
        .layout = prefilterPipelineLayout,
        .compute_shader = prefilterShader,
    });

    BindGroupHandle brdfBindGroup = device.createBindGroup(BindGroupDesc{
        .layout = brdfLayout,
        .entries =
            {
                BindGroupEntry{
                    .binding = 0,
                    .type = BindingType::StorageTexture,
                    .texture_view = ibl.brdf_lut.view,
                },
            },
    });
    BindGroupHandle irradianceBindGroup = device.createBindGroup(BindGroupDesc{
        .layout = cubeLayout,
        .entries =
            {
                BindGroupEntry{
                    .binding = 0,
                    .type = BindingType::CombinedImageSampler,
                    .texture_view = ibl.environment.view,
                    .sampler = ibl.sampler,
                },
                BindGroupEntry{
                    .binding = 1,
                    .type = BindingType::StorageTexture,
                    .texture_view = ibl.irradiance.view,
                },
            },
    });

    std::vector<TextureViewHandle> prefilterMipViews;
    std::vector<BindGroupHandle> prefilterBindGroups;
    for (uint32_t mip = 0; mip < ibl.prefilter_mip_levels; ++mip) {
        TextureViewHandle view = createCubeMipView(device, ibl.prefiltered.texture, ibl.prefiltered.format, mip);
        prefilterMipViews.push_back(view);
        prefilterBindGroups.push_back(device.createBindGroup(BindGroupDesc{
            .layout = cubeLayout,
            .entries =
                {
                    BindGroupEntry{
                        .binding = 0,
                        .type = BindingType::CombinedImageSampler,
                        .texture_view = ibl.environment.view,
                        .sampler = ibl.sampler,
                    },
                    BindGroupEntry{
                        .binding = 1,
                        .type = BindingType::StorageTexture,
                        .texture_view = view,
                    },
                },
        }));
    }

    auto* commands = device.getCommandList(commandList);
    const bool ready = ibl.irradiance.view && ibl.prefiltered.view && ibl.brdf_lut.view && brdfPipeline &&
                       irradiancePipeline && prefilterPipeline && brdfBindGroup && irradianceBindGroup &&
                       commands != nullptr;
    if (!ready) {
        return false;
    }

    commands->begin();
    std::array<TextureTransition, 3> storageTransitions = {{
        TextureTransition{.texture = ibl.brdf_lut.texture, .state = ResourceState::StorageReadWrite},
        TextureTransition{.texture = ibl.irradiance.texture, .state = ResourceState::StorageReadWrite},
        TextureTransition{.texture = ibl.prefiltered.texture, .state = ResourceState::StorageReadWrite},
    }};
    commands->transition(storageTransitions.data(), static_cast<uint32_t>(storageTransitions.size()));

    commands->setPipeline(brdfPipeline);
    commands->setBindGroup(0, brdfBindGroup);
    commands->dispatch((kBrdfLutSize + kComputeGroupSize - 1) / kComputeGroupSize,
                       (kBrdfLutSize + kComputeGroupSize - 1) / kComputeGroupSize);

    commands->setPipeline(irradiancePipeline);
    commands->setBindGroup(0, irradianceBindGroup);
    commands->dispatch((kIrradianceSize + kComputeGroupSize - 1) / kComputeGroupSize,
                       (kIrradianceSize + kComputeGroupSize - 1) / kComputeGroupSize,
                       6);

    commands->setPipeline(prefilterPipeline);
    const uint32_t sampledMaxPrefilterMip = maxSampledPrefilterMip(ibl.prefilter_mip_levels);
    for (uint32_t mip = 0; mip < ibl.prefilter_mip_levels; ++mip) {
        const float roughness =
            sampledMaxPrefilterMip > 0
                ? std::min(static_cast<float>(mip) / static_cast<float>(sampledMaxPrefilterMip), 1.0f)
                : 0.0f;
        const Vec4 constants{roughness, 0.0f, 0.0f, 0.0f};
        const uint32_t size = mipDimension(kPrefilterSize, mip);
        commands->setBindGroup(0, prefilterBindGroups[mip]);
        commands->pushConstants(shaderStageFlag(ShaderStage::Compute), 0, sizeof(constants), &constants);
        commands->dispatch(
            (size + kComputeGroupSize - 1) / kComputeGroupSize, (size + kComputeGroupSize - 1) / kComputeGroupSize, 6);
    }

    std::array<TextureTransition, 3> readTransitions = {{
        TextureTransition{.texture = ibl.brdf_lut.texture, .state = ResourceState::ShaderRead},
        TextureTransition{.texture = ibl.irradiance.texture, .state = ResourceState::ShaderRead},
        TextureTransition{.texture = ibl.prefiltered.texture, .state = ResourceState::ShaderRead},
    }};
    commands->transition(readTransitions.data(), static_cast<uint32_t>(readTransitions.size()));
    commands->end();
    device.submit(commandList);

    for (BindGroupHandle group : prefilterBindGroups) {
        device.destroyBindGroup(group);
    }
    for (TextureViewHandle view : prefilterMipViews) {
        device.destroyTextureView(view);
    }
    device.destroyBindGroup(irradianceBindGroup);
    device.destroyBindGroup(brdfBindGroup);
    device.destroyPipeline(prefilterPipeline);
    device.destroyPipeline(irradiancePipeline);
    device.destroyPipeline(brdfPipeline);
    device.destroyPipelineLayout(prefilterPipelineLayout);
    device.destroyPipelineLayout(cubePipelineLayout);
    device.destroyPipelineLayout(brdfPipelineLayout);
    device.destroyBindGroupLayout(cubeLayout);
    device.destroyBindGroupLayout(brdfLayout);
    device.destroyShader(prefilterShader);
    device.destroyShader(irradianceShader);
    device.destroyShader(brdfShader);
    return true;
}

void destroyIBL(Device& device, IBLResources& ibl)
{
    destroyTexture(device, ibl.brdf_lut);
    destroyTexture(device, ibl.prefiltered);
    destroyTexture(device, ibl.irradiance);
    destroyTexture(device, ibl.environment);
    if (ibl.sampler) {
        device.destroySampler(ibl.sampler);
    }
    ibl = {};
}

void destroyModel(Device& device, GpuModel& model)
{
    for (auto& primitive : model.primitives) {
        if (primitive.index_buffer) {
            device.destroyBuffer(primitive.index_buffer);
        }
        if (primitive.vertex_buffer) {
            device.destroyBuffer(primitive.vertex_buffer);
        }
    }
    for (auto& material : model.materials) {
        if (material.bind_group) {
            device.destroyBindGroup(material.bind_group);
        }
    }
    for (auto& texture : model.textures) {
        destroyTexture(device, texture);
    }
    for (auto& texture : model.defaults) {
        destroyTexture(device, texture);
    }
    model = {};
}

} // namespace

int main(int argc, char** argv)
{
    const std::filesystem::path gltfPath = findFile(argc,
                                                    argv,
                                                    {
                                                        "glTF/EnvironmentTest.gltf",
                                                        "examples/IBL/glTF/EnvironmentTest.gltf",
                                                    });
    const std::filesystem::path hdrPath = findFile(argc,
                                                   argv,
                                                   {
                                                       "newport_loft.hdr",
                                                       "examples/IBL/newport_loft.hdr",
                                                   });

    auto instance = BackendFactory::createInstance(BackendType::OpenGL);
    if (!instance) {
        std::printf("Failed to create OpenGL backend.\n");
        return 1;
    }

    tinyrhi_examples::Win32Window surface;
    if (!surface.create("TinyRHI IBL PBR", 1'280, 720)) {
        std::printf("Failed to create Win32 surface.\n");
        return 1;
    }

    if (!instance->init()) {
        std::printf("Failed to initialize TinyRHI instance.\n");
        return 1;
    }

    auto* device = instance->getDevice();
    const SurfaceHandle surfaceHandle = instance->createSurface(surface.nativeWindow());
    const SwapchainHandle swapchainHandle =
        device->createSwapchain(surfaceHandle, SwapchainDesc{.color_format = TextureFormat::RGBA8_SRGB});
    auto* swapchain = device->getSwapchain(swapchainHandle);
    if (swapchain == nullptr) {
        std::printf("Failed to create swapchain.\n");
        instance->shutdown();
        return 1;
    }

    const CommandListHandle commandListHandle = device->createCommandList();
    auto* commandList = device->getCommandList(commandListHandle);
    if (commandList == nullptr) {
        std::printf("Failed to create command list.\n");
        instance->shutdown();
        return 1;
    }
    auto& commands = *commandList;

    SamplerHandle materialSampler = device->createSampler(SamplerDesc{
        .min_filter = FilterMode::Linear,
        .mag_filter = FilterMode::Linear,
        .mip_filter = MipFilter::Linear,
        .address_u = AddressMode::Repeat,
        .address_v = AddressMode::Repeat,
        .address_w = AddressMode::ClampToEdge,
    });

    BindGroupLayoutDesc materialLayoutDesc{};
    for (uint32_t binding = 0; binding < 4; ++binding) {
        materialLayoutDesc.entries.push_back(BindGroupLayoutEntry{
            .binding = binding,
            .type = BindingType::CombinedImageSampler,
            .stages = shaderStageFlag(ShaderStage::Fragment),
        });
    }
    BindGroupLayoutHandle materialLayout = device->createBindGroupLayout(materialLayoutDesc);

    BindGroupLayoutDesc lightingLayoutDesc{};
    for (uint32_t binding = 0; binding < 6; ++binding) {
        lightingLayoutDesc.entries.push_back(BindGroupLayoutEntry{
            .binding = binding,
            .type = BindingType::CombinedImageSampler,
            .stages = shaderStageFlag(ShaderStage::Fragment),
        });
    }
    BindGroupLayoutHandle lightingBindGroupLayout = device->createBindGroupLayout(lightingLayoutDesc);

    PipelineLayoutDesc geometryPipelineLayoutDesc{};
    geometryPipelineLayoutDesc.bind_group_layouts.push_back(materialLayout);
    geometryPipelineLayoutDesc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Vertex) | shaderStageFlag(ShaderStage::Fragment),
        .offset = 0,
        .size = sizeof(GeometryPushConstants),
    });
    PipelineLayoutHandle geometryPipelineLayout = device->createPipelineLayout(geometryPipelineLayoutDesc);

    PipelineLayoutDesc lightingPipelineLayoutDesc{};
    lightingPipelineLayoutDesc.bind_group_layouts.push_back(lightingBindGroupLayout);
    lightingPipelineLayoutDesc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .offset = 0,
        .size = sizeof(LightingPushConstants),
    });
    PipelineLayoutHandle lightingPipelineLayout = device->createPipelineLayout(lightingPipelineLayoutDesc);

    ShaderHandle geometryVertexShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Vertex, .source = kGeometryVertexShader});
    ShaderHandle geometryFragmentShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Fragment, .source = kGeometryFragmentShader});
    ShaderHandle fullscreenVertexShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Vertex, .source = kFullscreenVertexShader});
    ShaderHandle lightingFragmentShader =
        device->createShader(ShaderDesc{.stage = ShaderStage::Fragment, .source = kLightingFragmentShader});

    PipelineDesc geometryPipelineDesc{};
    geometryPipelineDesc.topology = PrimitiveTopology::Triangle;
    geometryPipelineDesc.vertex_input = VertexInputDesc{
        .buffers =
            {
                VertexBufferLayoutDesc{
                    .binding = 0,
                    .stride = sizeof(Vertex),
                    .attributes =
                        {
                            VertexAttributeDesc{
                                .location = 0,
                                .format = VertexFormat::Float3,
                                .offset = offsetof(Vertex, position),
                            },
                            VertexAttributeDesc{
                                .location = 1,
                                .format = VertexFormat::Float3,
                                .offset = offsetof(Vertex, normal),
                            },
                            VertexAttributeDesc{
                                .location = 2,
                                .format = VertexFormat::Float4,
                                .offset = offsetof(Vertex, tangent),
                            },
                            VertexAttributeDesc{
                                .location = 3,
                                .format = VertexFormat::Float2,
                                .offset = offsetof(Vertex, uv),
                            },
                        },
                },
            },
    };
    geometryPipelineDesc.layout = geometryPipelineLayout;
    geometryPipelineDesc.vertex_shader = geometryVertexShader;
    geometryPipelineDesc.fragment_shader = geometryFragmentShader;
    geometryPipelineDesc.render_target_state.color_targets.push_back(
        ColorTargetState{.format = TextureFormat::RGBA16F});
    geometryPipelineDesc.render_target_state.color_targets.push_back(
        ColorTargetState{.format = TextureFormat::RGBA16F});
    geometryPipelineDesc.render_target_state.color_targets.push_back(
        ColorTargetState{.format = TextureFormat::RGBA16F});
    geometryPipelineDesc.render_target_state.has_depth_stencil = true;
    geometryPipelineDesc.render_target_state.depth_stencil_format = TextureFormat::Depth24Stencil8;
    geometryPipelineDesc.depth_state.enabled = true;
    geometryPipelineDesc.depth_state.write_enabled = true;
    geometryPipelineDesc.depth_state.compare = CompareOp::Less;
    geometryPipelineDesc.raster_state.cull_mode = CullMode::None;
    PipelineHandle geometryPipeline = device->createPipeline(geometryPipelineDesc);

    PipelineDesc lightingPipelineDesc{};
    lightingPipelineDesc.topology = PrimitiveTopology::Triangle;
    lightingPipelineDesc.layout = lightingPipelineLayout;
    lightingPipelineDesc.vertex_shader = fullscreenVertexShader;
    lightingPipelineDesc.fragment_shader = lightingFragmentShader;
    lightingPipelineDesc.render_target_state.color_targets.push_back(
        ColorTargetState{.format = TextureFormat::RGBA8_SRGB});
    lightingPipelineDesc.depth_state.enabled = false;
    PipelineHandle lightingPipeline = device->createPipeline(lightingPipelineDesc);

    GpuModel model;
    IBLResources ibl;
    const bool resourcesReady =
        materialSampler && materialLayout && lightingBindGroupLayout && geometryPipelineLayout &&
        lightingPipelineLayout && geometryVertexShader && geometryFragmentShader && fullscreenVertexShader &&
        lightingFragmentShader && geometryPipeline && lightingPipeline &&
        loadGltfModel(*device, commandListHandle, gltfPath, materialLayout, materialSampler, model) &&
        precomputeIBL(*device, commandListHandle, hdrPath, ibl);

    if (!resourcesReady) {
        std::printf("Failed to create IBL PBR resources.\n");
        destroyIBL(*device, ibl);
        destroyModel(*device, model);
        instance->shutdown();
        return 1;
    }

    const Vec3 boundsCenter = (model.bounds.min + model.bounds.max) * 0.5f;
    const Vec3 boundsExtent = model.bounds.max - model.bounds.min;
    const float radius = std::max(4.0f, length(boundsExtent) * 0.55f);
    const HWND window = surface.hwnd();
    const Vec3 worldUp{0.0f, 1.0f, 0.0f};
    FreeCamera camera;
    camera.position = boundsCenter + Vec3{0.0f, radius * 0.25f, radius * 2.0f};
    {
        const Vec3 initialForward = normalize(boundsCenter - camera.position);
        camera.yaw = std::atan2(initialForward.x, initialForward.z);
        camera.pitch = std::asin(std::clamp(initialForward.y, -1.0f, 1.0f));
    }
    const float moveSpeed = std::max(3.0f, radius * 1.25f);
    const float mouseSensitivity = 0.0025f;
    bool mouseLookActive = false;
    auto previousFrame = std::chrono::steady_clock::now();
    GBufferResources gbuffer;

    while (surface.pollEvents() && !surface.shouldClose()) {
        swapchain->resize(surface.getWidth(), surface.getHeight());
        SwapchainFrame frame{};
        if (!device->beginFrame(swapchainHandle, frame)) {
            break;
        }
        if (!ensureGBuffer(*device, frame.width, frame.height, lightingBindGroupLayout, ibl, gbuffer)) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - previousFrame).count();
        previousFrame = now;
        dt = std::clamp(dt, 0.0f, 0.05f);

        const bool windowFocused = GetForegroundWindow() == window;
        if (!windowFocused && mouseLookActive) {
            mouseLookActive = false;
            ReleaseCapture();
            while (ShowCursor(TRUE) < 0) {}
        }
        if (windowFocused) {
            const bool rightButtonDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            if (rightButtonDown && !mouseLookActive) {
                mouseLookActive = true;
                SetCapture(window);
                while (ShowCursor(FALSE) >= 0) {}
                const POINT center = clientCenterToScreen(window);
                SetCursorPos(center.x, center.y);
            } else if (!rightButtonDown && mouseLookActive) {
                mouseLookActive = false;
                ReleaseCapture();
                while (ShowCursor(TRUE) < 0) {}
            }

            if (mouseLookActive) {
                const POINT center = clientCenterToScreen(window);
                POINT cursor{};
                if (GetCursorPos(&cursor)) {
                    const LONG deltaX = cursor.x - center.x;
                    const LONG deltaY = cursor.y - center.y;
                    if (deltaX != 0 || deltaY != 0) {
                        camera.yaw += static_cast<float>(deltaX) * mouseSensitivity;
                        camera.pitch = std::clamp(camera.pitch - static_cast<float>(deltaY) * mouseSensitivity,
                                                  -1.53f,
                                                  1.53f);
                    }
                }
                SetCursorPos(center.x, center.y);
            }

            const Vec3 forward = forwardFromYawPitch(camera.yaw, camera.pitch);
            const Vec3 right = normalize(cross(forward, worldUp));
            const Vec3 forwardOnPlane = normalize(Vec3{forward.x, 0.0f, forward.z});
            float speed = moveSpeed * dt;
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                speed *= 4.0f;
            }
            if (GetAsyncKeyState('W') & 0x8000) {
                camera.position = camera.position + forwardOnPlane * speed;
            }
            if (GetAsyncKeyState('S') & 0x8000) {
                camera.position = camera.position - forwardOnPlane * speed;
            }
            if (GetAsyncKeyState('D') & 0x8000) {
                camera.position = camera.position + right * speed;
            }
            if (GetAsyncKeyState('A') & 0x8000) {
                camera.position = camera.position - right * speed;
            }
            if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
                camera.position.y += speed;
            }
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                camera.position.y -= speed;
            }
        }

        const float aspect =
            frame.height > 0 ? static_cast<float>(frame.width) / static_cast<float>(frame.height) : 1.0f;
        const Mat4 projection = perspective(60.0f * kPi / 180.0f, aspect, 0.05f, radius * 8.0f);
        const Vec3 forward = forwardFromYawPitch(camera.yaw, camera.pitch);
        const Mat4 view = lookAt(camera.position, camera.position + forward, worldUp);
        const Mat4 viewProjection = multiply(projection, view);
        Mat4 inverseViewProjection{};
        invertMatrix(viewProjection, inverseViewProjection);

        RenderPassBeginInfo gbufferPass{};
        gbufferPass.color_attachments.push_back(ColorAttachmentDesc{
            .view = gbuffer.position_roughness.view,
            .load_op = LoadOp::Clear,
            .store_op = StoreOp::Store,
            .clear_color = ClearColor{0.0f, 0.0f, 0.0f, 0.0f},
        });
        gbufferPass.color_attachments.push_back(ColorAttachmentDesc{
            .view = gbuffer.normal_ao.view,
            .load_op = LoadOp::Clear,
            .store_op = StoreOp::Store,
            .clear_color = ClearColor{0.0f, 0.0f, 0.0f, 0.0f},
        });
        gbufferPass.color_attachments.push_back(ColorAttachmentDesc{
            .view = gbuffer.albedo_metallic.view,
            .load_op = LoadOp::Clear,
            .store_op = StoreOp::Store,
            .clear_color = ClearColor{0.0f, 0.0f, 0.0f, 0.0f},
        });
        gbufferPass.has_depth_stencil_attachment = true;
        gbufferPass.depth_stencil_attachment = DepthStencilAttachmentDesc{
            .view = gbuffer.depth.view,
            .depth_load_op = LoadOp::Clear,
            .depth_store_op = StoreOp::Store,
            .clear_depth = 1.0f,
        };
        gbufferPass.width = frame.width;
        gbufferPass.height = frame.height;

        RenderPassBeginInfo lightingPass{};
        lightingPass.color_attachments.push_back(ColorAttachmentDesc{
            .view = frame.color_view,
            .load_op = LoadOp::Clear,
            .store_op = StoreOp::Store,
            .clear_color = ClearColor{0.0f, 0.0f, 0.0f, 1.0f},
        });
        lightingPass.width = frame.width;
        lightingPass.height = frame.height;

        commands.begin();
        commands.beginRenderPass(gbufferPass);
        commands.setPipeline(geometryPipeline);
        for (const PrimitiveResource& primitive : model.primitives) {
            const MaterialResource& material = model.materials[static_cast<size_t>(primitive.material_index)];
            const GeometryPushConstants constants{
                .model = primitive.model,
                .view_projection = viewProjection,
                .base_color = material.factors.base_color,
                .properties = material.factors.properties,
            };
            commands.setBindGroup(0, material.bind_group);
            commands.pushConstants(shaderStageFlag(ShaderStage::Vertex) | shaderStageFlag(ShaderStage::Fragment),
                                   0,
                                   sizeof(constants),
                                   &constants);
            commands.setVertexBuffer(0, primitive.vertex_buffer);
            commands.setIndexBuffer(primitive.index_buffer, IndexFormat::UInt32);
            commands.drawIndexed(primitive.index_count);
        }
        commands.endRenderPass();

        std::array<TextureTransition, 3> gbufferReadTransitions = {{
            TextureTransition{.texture = gbuffer.position_roughness.texture, .state = ResourceState::ShaderRead},
            TextureTransition{.texture = gbuffer.normal_ao.texture, .state = ResourceState::ShaderRead},
            TextureTransition{.texture = gbuffer.albedo_metallic.texture, .state = ResourceState::ShaderRead},
        }};
        commands.transition(gbufferReadTransitions.data(), static_cast<uint32_t>(gbufferReadTransitions.size()));

        const LightingPushConstants lightingConstants{
            .inverse_view_projection = inverseViewProjection,
            .camera_exposure = Vec4{camera.position.x, camera.position.y, camera.position.z, 1.0f},
            .light_direction_intensity = Vec4{-0.45f, -0.8f, -0.35f, 3.0f},
            .light_color_ibl = Vec4{1.0f, 0.96f, 0.9f, 1.4f},
            .params = Vec4{static_cast<float>(maxSampledPrefilterMip(ibl.prefilter_mip_levels)), 0.0f, 0.0f, 0.0f},
        };

        commands.beginRenderPass(lightingPass);
        commands.setPipeline(lightingPipeline);
        commands.setBindGroup(0, gbuffer.lighting_bind_group);
        commands.pushConstants(
            shaderStageFlag(ShaderStage::Fragment), 0, sizeof(lightingConstants), &lightingConstants);
        commands.draw(3);
        commands.endRenderPass();
        commands.end();

        device->submit(commandListHandle, &frame);
        device->present(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (mouseLookActive) {
        ReleaseCapture();
        while (ShowCursor(TRUE) < 0) {}
    }

    destroyGBuffer(*device, gbuffer);
    destroyIBL(*device, ibl);
    destroyModel(*device, model);
    device->destroyPipeline(lightingPipeline);
    device->destroyPipeline(geometryPipeline);
    device->destroyShader(lightingFragmentShader);
    device->destroyShader(fullscreenVertexShader);
    device->destroyShader(geometryFragmentShader);
    device->destroyShader(geometryVertexShader);
    device->destroyPipelineLayout(lightingPipelineLayout);
    device->destroyPipelineLayout(geometryPipelineLayout);
    device->destroyBindGroupLayout(lightingBindGroupLayout);
    device->destroyBindGroupLayout(materialLayout);
    device->destroySampler(materialSampler);
    instance->shutdown();
    return 0;
}
