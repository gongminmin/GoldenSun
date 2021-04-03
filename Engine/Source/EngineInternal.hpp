#pragma once

#include <d3d12.h>
#include <vector>

namespace GoldenSun
{
    class Mesh;

    // Must match PbrMaterial in shader
    struct PbrMaterialBuffer
    {
        alignas(4) DirectX::XMFLOAT3 albedo{0, 0, 0};
        alignas(4) float opacity{1};
        alignas(4) DirectX::XMFLOAT3 emissive{0, 0, 0};
        alignas(4) float metallic{0};
        alignas(4) float roughness{1};
        alignas(4) float alpha_cutoff{0};
        alignas(4) float normal_scale{1};
        alignas(4) float occlusion_strength{1};
        alignas(4) uint32_t transparent{0};
        alignas(4) uint32_t two_sided{0};
        alignas(4) uint8_t paddings[8]{};
    };
    static_assert(sizeof(PbrMaterialBuffer) % 16 == 0);

    // Must match Light in shader
    struct LightBuffer
    {
        alignas(4) DirectX::XMFLOAT3 position;
        alignas(4) DirectX::XMFLOAT3 color;
        alignas(4) DirectX::XMFLOAT3 falloff;
        alignas(4) uint32_t shadowing;
        alignas(4) uint8_t paddings[24]{};
    };
    static_assert(sizeof(LightBuffer) % 16 == 0);
    static_assert(sizeof(LightBuffer) % 64 == 0); // TODO: Remove this after GpuMemoryAllocator can allocate an aligned address

    class EngineInternal final
    {
    public:
        static std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> GeometryDescs(Mesh const& mesh);
        static PbrMaterialBuffer const& Buffer(PbrMaterial const& material);
        static LightBuffer const& Buffer(PointLight const& light);
    };
} // namespace GoldenSun
