#pragma once

#include <DirectXMath.h>

struct ID3D12Resource;

namespace GoldenSun
{
    class EngineInternal;

    class GOLDEN_SUN_API PbrMaterial final
    {
        friend class EngineInternal;

        DISALLOW_COPY_AND_ASSIGN(PbrMaterial)

    public:
        static float constexpr MaxGlossiness = 8192;

        enum class TextureSlot : uint32_t
        {
            Albedo,
            MetallicGlossiness,
            Emissive,
            Normal,
            Occlusion,

            Num
        };

    public:
        PbrMaterial();
        ~PbrMaterial() noexcept;

        PbrMaterial(PbrMaterial&& other) noexcept;
        PbrMaterial& operator=(PbrMaterial&& other) noexcept;

        PbrMaterial Clone() const;

        DirectX::XMFLOAT3& Albedo() noexcept;
        DirectX::XMFLOAT3 const& Albedo() const noexcept;

        float& Opacity() noexcept;
        float Opacity() const noexcept;

        float& Metallic() noexcept;
        float Metallic() const noexcept;
        float& Glossiness() noexcept;
        float Glossiness() const noexcept;

        DirectX::XMFLOAT3& Emissive() noexcept;
        DirectX::XMFLOAT3 const& Emissive() const noexcept;

        float& AlphaCutoff() noexcept;
        float AlphaCutoff() const noexcept;

        bool& Transparent() noexcept;
        bool Transparent() const noexcept;
        bool& TwoSided() noexcept;
        bool TwoSided() const noexcept;

        float& NormalScale() noexcept;
        float NormalScale() const noexcept;
        float& OcclusionStrength() noexcept;
        float OcclusionStrength() const noexcept;

        void Texture(TextureSlot slot, ID3D12Resource* value) noexcept;
        ID3D12Resource* Texture(TextureSlot slot) const noexcept;

    private:
        class Impl;
        Impl* impl_;
    };
} // namespace GoldenSun
