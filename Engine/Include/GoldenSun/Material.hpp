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

        void Albedo(DirectX::XMFLOAT3 const& value) noexcept;
        DirectX::XMFLOAT3 const& Albedo() const noexcept;

        void Opacity(float value) noexcept;
        float Opacity() const noexcept;

        void Metallic(float value) noexcept;
        float Metallic() const noexcept;
        void Glossiness(float value) noexcept;
        float Glossiness() const noexcept;

        void Emissive(DirectX::XMFLOAT3 const& value) noexcept;
        DirectX::XMFLOAT3 const& Emissive() const noexcept;

        void AlphaCutoff(float value) noexcept;
        float AlphaCutoff() const noexcept;

        void Transparent(bool value) noexcept;
        bool Transparent() const noexcept;
        void TwoSided(bool value) noexcept;
        bool TwoSided() const noexcept;

        void NormalScale(float value) noexcept;
        float NormalScale() const noexcept;
        void OcclusionStrength(float value) noexcept;
        float OcclusionStrength() const noexcept;

        void Texture(TextureSlot slot, ID3D12Resource* value) noexcept;
        ID3D12Resource* Texture(TextureSlot slot) const noexcept;

    private:
        class Impl;
        Impl* impl_;
    };
} // namespace GoldenSun
