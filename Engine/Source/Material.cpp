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

        XMFLOAT3& Albedo() noexcept
        {
            return buffer_.albedo;
        }

        XMFLOAT3 const& Albedo() const noexcept
        {
            return buffer_.albedo;
        }

        float& Opacity() noexcept
        {
            return buffer_.opacity;
        }

        float Opacity() const noexcept
        {
            return buffer_.opacity;
        }

        float& Metallic() noexcept
        {
            return buffer_.metallic;
        }

        float Metallic() const noexcept
        {
            return buffer_.metallic;
        }

        float& Roughness() noexcept
        {
            return buffer_.roughness;
        }

        float Roughness() const noexcept
        {
            return buffer_.roughness;
        }

        XMFLOAT3& Emissive() noexcept
        {
            return buffer_.emissive;
        }

        XMFLOAT3 const& Emissive() const noexcept
        {
            return buffer_.emissive;
        }

        float& AlphaCutoff() noexcept
        {
            return buffer_.alpha_cutoff;
        }

        float AlphaCutoff() const noexcept
        {
            return buffer_.alpha_cutoff;
        }

        bool& Transparent() noexcept
        {
            return reinterpret_cast<bool&>(buffer_.transparent);
        }

        bool Transparent() const noexcept
        {
            return buffer_.transparent;
        }

        bool& TwoSided() noexcept
        {
            return reinterpret_cast<bool&>(buffer_.two_sided);
        }

        bool TwoSided() const noexcept
        {
            return buffer_.two_sided;
        }

        float& NormalScale() noexcept
        {
            return buffer_.normal_scale;
        }

        float NormalScale() const noexcept
        {
            return buffer_.normal_scale;
        }

        float& OcclusionStrength() noexcept
        {
            return buffer_.occlusion_strength;
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

    XMFLOAT3& PbrMaterial::Albedo() noexcept
    {
        return impl_->Albedo();
    }

    XMFLOAT3 const& PbrMaterial::Albedo() const noexcept
    {
        return impl_->Albedo();
    }

    float& PbrMaterial::Opacity() noexcept
    {
        return impl_->Opacity();
    }

    float PbrMaterial::Opacity() const noexcept
    {
        return impl_->Opacity();
    }

    float& PbrMaterial::Metallic() noexcept
    {
        return impl_->Metallic();
    }

    float PbrMaterial::Metallic() const noexcept
    {
        return impl_->Metallic();
    }

    float& PbrMaterial::Roughness() noexcept
    {
        return impl_->Roughness();
    }

    float PbrMaterial::Roughness() const noexcept
    {
        return impl_->Roughness();
    }

    XMFLOAT3& PbrMaterial::Emissive() noexcept
    {
        return impl_->Emissive();
    }

    XMFLOAT3 const& PbrMaterial::Emissive() const noexcept
    {
        return impl_->Emissive();
    }

    float& PbrMaterial::AlphaCutoff() noexcept
    {
        return impl_->AlphaCutoff();
    }

    float PbrMaterial::AlphaCutoff() const noexcept
    {
        return impl_->AlphaCutoff();
    }

    bool& PbrMaterial::Transparent() noexcept
    {
        return impl_->Transparent();
    }

    bool PbrMaterial::Transparent() const noexcept
    {
        return impl_->Transparent();
    }

    bool& PbrMaterial::TwoSided() noexcept
    {
        return impl_->TwoSided();
    }

    bool PbrMaterial::TwoSided() const noexcept
    {
        return impl_->TwoSided();
    }

    float& PbrMaterial::NormalScale() noexcept
    {
        return impl_->NormalScale();
    }

    float PbrMaterial::NormalScale() const noexcept
    {
        return impl_->NormalScale();
    }

    float& PbrMaterial::OcclusionStrength() noexcept
    {
        return impl_->OcclusionStrength();
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
