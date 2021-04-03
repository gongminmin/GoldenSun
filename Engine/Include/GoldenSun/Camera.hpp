#pragma once

#include <DirectXMath.h>

namespace GoldenSun
{
    class EngineInternal;

    class GOLDEN_SUN_API Camera final
    {
        friend class EngineInternal;

        DISALLOW_COPY_AND_ASSIGN(Camera)

    public:
        Camera();
        ~Camera() noexcept;

        Camera(Camera&& other) noexcept;
        Camera& operator=(Camera&& other) noexcept;

        Camera Clone() const;

        DirectX::XMFLOAT3& Eye() noexcept;
        DirectX::XMFLOAT3 const& Eye() const noexcept;

        DirectX::XMFLOAT3& LookAt() noexcept;
        DirectX::XMFLOAT3 const& LookAt() const noexcept;

        DirectX::XMFLOAT3& Up() noexcept;
        DirectX::XMFLOAT3 const& Up() const noexcept;

        float& Fov() noexcept;
        float Fov() const noexcept;

        float& NearPlane() noexcept;
        float NearPlane() const noexcept;

        float& FarPlane() noexcept;
        float FarPlane() const noexcept;

    private:
        class Impl;
        Impl* impl_;
    };
} // namespace GoldenSun
