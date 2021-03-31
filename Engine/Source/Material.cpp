#include "pch.hpp"

#include <GoldenSun/Material.hpp>

#include <GoldenSun/Util.hpp>

#include "EngineInternal.hpp"

using namespace DirectX;

namespace GoldenSun
{
    class PbrMaterial::Impl
    {
        friend class PbrMaterial;

        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        void Albedo(DirectX::XMFLOAT3 const& value) noexcept
        {
            buffer_.albedo = value;
        }

        DirectX::XMFLOAT3 const& Albedo() const noexcept
        {
            return buffer_.albedo;
        }

        void Opacity(float value) noexcept
        {
            buffer_.opacity = value;
        }

        float Opacity() const noexcept
        {
            return buffer_.opacity;
        }

        void Metallic(float value) noexcept
        {
            buffer_.metallic = value;
        }

        float Metallic() const noexcept
        {
            return buffer_.metallic;
        }

        void Glossiness(float value) noexcept
        {
            buffer_.glossiness = value;
        }

        float Glossiness() const noexcept
        {
            return buffer_.glossiness;
        }

        void Emissive(DirectX::XMFLOAT3 const& value) noexcept
        {
            buffer_.emissive = value;
        }

        DirectX::XMFLOAT3 const& Emissive() const noexcept
        {
            return buffer_.emissive;
        }

        void AlphaCutoff(float value) noexcept
        {
            buffer_.alpha_cutoff = value;
        }

        float AlphaCutoff() const noexcept
        {
            return buffer_.alpha_cutoff;
        }

        void Transparent(bool value) noexcept
        {
            buffer_.transparent = value;
        }

        bool Transparent() const noexcept
        {
            return buffer_.transparent;
        }

        void TwoSided(bool value) noexcept
        {
            buffer_.two_sided = value;
        }

        bool TwoSided() const noexcept
        {
            return buffer_.two_sided;
        }

        void NormalScale(float value) noexcept
        {
            buffer_.normal_scale = value;
        }

        float NormalScale() const noexcept
        {
            return buffer_.normal_scale;
        }

        void OcclusionStrength(float value) noexcept
        {
            buffer_.occlusion_strength = value;
        }

        float OcclusionStrength() const noexcept
        {
            return buffer_.occlusion_strength;
        }

        void Texture(TextureSlot slot, ID3D12Resource* value) noexcept
        {
            textures_[std::to_underlying(slot)] = value;
        }

        ID3D12Resource* Texture(TextureSlot slot) const noexcept
        {
            return textures_[std::to_underlying(slot)].Get();
        }

        PbrMaterialBuffer const& Buffer() const noexcept
        {
            return buffer_;
        }

    private:
        PbrMaterialBuffer buffer_{};
        std::array<ComPtr<ID3D12Resource>, std::to_underlying(TextureSlot::Num)> textures_;
    };


    PbrMaterial::PbrMaterial() : impl_(new Impl)
    {
    }

    PbrMaterial::~PbrMaterial() noexcept
    {
        delete impl_;
        impl_ = nullptr;
    }

    PbrMaterial::PbrMaterial(PbrMaterial&& other) noexcept : impl_(std::move(other.impl_))
    {
        other.impl_ = nullptr;
    }

    PbrMaterial& PbrMaterial::operator=(PbrMaterial&& other) noexcept
    {
        if (this != &other)
        {
            impl_ = std::move(other.impl_);
            other.impl_ = nullptr;
        }
        return *this;
    }

    PbrMaterial PbrMaterial::Clone() const
    {
        PbrMaterial mtl;
        mtl.impl_->buffer_ = impl_->buffer_;
        mtl.impl_->textures_ = impl_->textures_;
        return mtl;
    }

    void PbrMaterial::Albedo(DirectX::XMFLOAT3 const& value) noexcept
    {
        impl_->Albedo(value);
    }

    DirectX::XMFLOAT3 const& PbrMaterial::Albedo() const noexcept
    {
        return impl_->Albedo();
    }

    void PbrMaterial::Opacity(float value) noexcept
    {
        impl_->Opacity(value);
    }

    float PbrMaterial::Opacity() const noexcept
    {
        return impl_->Opacity();
    }

    void PbrMaterial::Metallic(float value) noexcept
    {
        impl_->Metallic(value);
    }

    float PbrMaterial::Metallic() const noexcept
    {
        return impl_->Metallic();
    }

    void PbrMaterial::Glossiness(float value) noexcept
    {
        impl_->Glossiness(value);
    }

    float PbrMaterial::Glossiness() const noexcept
    {
        return impl_->Glossiness();
    }

    void PbrMaterial::Emissive(DirectX::XMFLOAT3 const& value) noexcept
    {
        impl_->Emissive(value);
    }

    DirectX::XMFLOAT3 const& PbrMaterial::Emissive() const noexcept
    {
        return impl_->Emissive();
    }

    void PbrMaterial::AlphaCutoff(float value) noexcept
    {
        impl_->AlphaCutoff(value);
    }

    float PbrMaterial::AlphaCutoff() const noexcept
    {
        return impl_->AlphaCutoff();
    }

    void PbrMaterial::Transparent(bool value) noexcept
    {
        impl_->Transparent(value);
    }

    bool PbrMaterial::Transparent() const noexcept
    {
        return impl_->Transparent();
    }

    void PbrMaterial::TwoSided(bool value) noexcept
    {
        impl_->TwoSided(value);
    }

    bool PbrMaterial::TwoSided() const noexcept
    {
        return impl_->TwoSided();
    }

    void PbrMaterial::NormalScale(float value) noexcept
    {
        impl_->NormalScale(value);
    }

    float PbrMaterial::NormalScale() const noexcept
    {
        return impl_->NormalScale();
    }

    void PbrMaterial::OcclusionStrength(float value) noexcept
    {
        impl_->OcclusionStrength(value);
    }

    float PbrMaterial::OcclusionStrength() const noexcept
    {
        return impl_->OcclusionStrength();
    }

    void PbrMaterial::Texture(TextureSlot slot, ID3D12Resource* value) noexcept
    {
        impl_->Texture(slot, value);
    }

    ID3D12Resource* PbrMaterial::Texture(TextureSlot slot) const noexcept
    {
        return impl_->Texture(slot);
    }


    PbrMaterialBuffer const& EngineInternal::Buffer(PbrMaterial const& material)
    {
        return material.impl_->Buffer();
    }
} // namespace GoldenSun
