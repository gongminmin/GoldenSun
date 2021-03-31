#include "pch.hpp"

#include <GoldenSun/Material.hpp>

#include "EngineInternal.hpp"

using namespace DirectX;

namespace GoldenSun
{
    class PointLight::Impl
    {
        friend class PointLight;

        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        void Position(DirectX::XMFLOAT3 const& value) noexcept
        {
            buffer_.position = value;
        }

        DirectX::XMFLOAT3 const& Position() const noexcept
        {
            return buffer_.position;
        }

        void Color(DirectX::XMFLOAT3 const& value) noexcept
        {
            buffer_.color = value;
        }

        DirectX::XMFLOAT3 const& Color() const noexcept
        {
            return buffer_.color;
        }

        void Falloff(DirectX::XMFLOAT3 const& value) noexcept
        {
            buffer_.falloff = value;
        }

        DirectX::XMFLOAT3 const& Falloff() const noexcept
        {
            return buffer_.falloff;
        }

        void Shadowing(bool value) noexcept
        {
            buffer_.shadowing = value;
        }

        bool Shadowing() const noexcept
        {
            return buffer_.shadowing;
        }

        LightBuffer const& Buffer() const noexcept
        {
            return buffer_;
        }

    private:
        LightBuffer buffer_{};
    };


    PointLight::PointLight() : impl_(new Impl)
    {
    }

    PointLight::~PointLight() noexcept
    {
        delete impl_;
        impl_ = nullptr;
    }

    PointLight::PointLight(PointLight&& other) noexcept : impl_(std::move(other.impl_))
    {
        other.impl_ = nullptr;
    }

    PointLight& PointLight::operator=(PointLight&& other) noexcept
    {
        if (this != &other)
        {
            impl_ = std::move(other.impl_);
            other.impl_ = nullptr;
        }
        return *this;
    }

    PointLight PointLight::Clone() const
    {
        PointLight light;
        light.impl_->buffer_ = impl_->buffer_;
        return light;
    }

    void PointLight::Position(DirectX::XMFLOAT3 const& value) noexcept
    {
        impl_->Position(value);
    }

    DirectX::XMFLOAT3 const& PointLight::Position() const noexcept
    {
        return impl_->Position();
    }

    void PointLight::Color(DirectX::XMFLOAT3 const& value) noexcept
    {
        impl_->Color(value);
    }

    DirectX::XMFLOAT3 const& PointLight::Color() const noexcept
    {
        return impl_->Color();
    }

    void PointLight::Falloff(DirectX::XMFLOAT3 const& value) noexcept
    {
        impl_->Falloff(value);
    }

    DirectX::XMFLOAT3 const& PointLight::Falloff() const noexcept
    {
        return impl_->Falloff();
    }

    void PointLight::Shadowing(bool value) noexcept
    {
        impl_->Shadowing(value);
    }

    bool PointLight::Shadowing() const noexcept
    {
        return impl_->Shadowing();
    }


    LightBuffer const& EngineInternal::Buffer(PointLight const& light)
    {
        return light.impl_->Buffer();
    }
} // namespace GoldenSun
