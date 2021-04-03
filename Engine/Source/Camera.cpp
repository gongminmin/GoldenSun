#include "pch.hpp"

#include <GoldenSun/Camera.hpp>

#include "EngineInternal.hpp"

using namespace DirectX;

namespace GoldenSun
{
    class Camera::Impl
    {
        friend class Camera;

        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        void Clone(Impl& target)
        {
            target.eye_ = eye_;
            target.look_at_ = look_at_;
            target.up_ = up_;

            target.fov_ = fov_;

            target.near_plane_ = near_plane_;
            target.far_plane_ = far_plane_;
        }

        XMFLOAT3& Eye() noexcept
        {
            return eye_;
        }

        XMFLOAT3 const& Eye() const noexcept
        {
            return eye_;
        }

        XMFLOAT3& LookAt() noexcept
        {
            return look_at_;
        }

        XMFLOAT3 const& LookAt() const noexcept
        {
            return look_at_;
        }

        XMFLOAT3& Up() noexcept
        {
            return up_;
        }

        XMFLOAT3 const& Up() const noexcept
        {
            return up_;
        }

        float& Fov() noexcept
        {
            return fov_;
        }

        float Fov() const noexcept
        {
            return fov_;
        }

        float& NearPlane() noexcept
        {
            return near_plane_;
        }

        float NearPlane() const noexcept
        {
            return near_plane_;
        }

        float& FarPlane() noexcept
        {
            return far_plane_;
        }

        float FarPlane() const noexcept
        {
            return far_plane_;
        }

    private:
        XMFLOAT3 eye_;
        XMFLOAT3 look_at_;
        XMFLOAT3 up_;

        float fov_;

        float near_plane_;
        float far_plane_;
    };


    Camera::Camera() : impl_(new Impl)
    {
    }

    Camera::~Camera() noexcept
    {
        delete impl_;
        impl_ = nullptr;
    }

    Camera::Camera(Camera&& other) noexcept : impl_(std::move(other.impl_))
    {
        other.impl_ = nullptr;
    }

    Camera& Camera::operator=(Camera&& other) noexcept
    {
        if (this != &other)
        {
            impl_ = std::move(other.impl_);
            other.impl_ = nullptr;
        }
        return *this;
    }

    Camera Camera::Clone() const
    {
        Camera camera;
        impl_->Clone(*camera.impl_);
        return camera;
    }

    XMFLOAT3& Camera::Eye() noexcept
    {
        return impl_->Eye();
    }

    XMFLOAT3 const& Camera::Eye() const noexcept
    {
        return impl_->Eye();
    }

    XMFLOAT3& Camera::LookAt() noexcept
    {
        return impl_->LookAt();
    }

    XMFLOAT3 const& Camera::LookAt() const noexcept
    {
        return impl_->LookAt();
    }

    XMFLOAT3& Camera::Up() noexcept
    {
        return impl_->Up();
    }

    XMFLOAT3 const& Camera::Up() const noexcept
    {
        return impl_->Up();
    }

    float& Camera::Fov() noexcept
    {
        return impl_->Fov();
    }

    float Camera::Fov() const noexcept
    {
        return impl_->Fov();
    }

    float& Camera::NearPlane() noexcept
    {
        return impl_->NearPlane();
    }

    float Camera::NearPlane() const noexcept
    {
        return impl_->NearPlane();
    }

    float& Camera::FarPlane() noexcept
    {
        return impl_->FarPlane();
    }

    float Camera::FarPlane() const noexcept
    {
        return impl_->FarPlane();
    }
} // namespace GoldenSun
