#include "pch.hpp"

#include <GoldenSun/Light.hpp>

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

        void Clone(Impl& target) const
        {
            target.buffer_ = buffer_;
        }

        XMFLOAT3& Position() noexcept
        {
            return buffer_.position;
        }

        XMFLOAT3 const& Position() const noexcept
        {
            return buffer_.position;
        }

        XMFLOAT3& Color() noexcept
        {
            return buffer_.color;
        }

        XMFLOAT3 const& Color() const noexcept
        {
            return buffer_.color;
        }

        XMFLOAT3& Falloff() noexcept
        {
            return buffer_.falloff;
        }

        XMFLOAT3 const& Falloff() const noexcept
        {
            return buffer_.falloff;
        }

        bool& Shadowing() noexcept
        {
            return reinterpret_cast<bool&>(buffer_.shadowing);
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
        impl_->Clone(*light.impl_);
        return light;
    }

    XMFLOAT3& PointLight::Position() noexcept
    {
        return impl_->Position();
    }

    XMFLOAT3 const& PointLight::Position() const noexcept
    {
        return impl_->Position();
    }

    XMFLOAT3& PointLight::Color() noexcept
    {
        return impl_->Color();
    }

    XMFLOAT3 const& PointLight::Color() const noexcept
    {
        return impl_->Color();
    }

    XMFLOAT3& PointLight::Falloff() noexcept
    {
        return impl_->Falloff();
    }

    XMFLOAT3 const& PointLight::Falloff() const noexcept
    {
        return impl_->Falloff();
    }

    bool& PointLight::Shadowing() noexcept
    {
        return impl_->Shadowing();
    }

    bool PointLight::Shadowing() const noexcept
    {
        return impl_->Shadowing();
    }


    LightBuffer const& EngineInternal::Buffer(PointLight const& light) noexcept
    {
        return light.impl_->Buffer();
    }
} // namespace GoldenSun
