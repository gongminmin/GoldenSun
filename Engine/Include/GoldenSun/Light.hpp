#pragma once

#include <DirectXMath.h>

namespace GoldenSun
{
    class EngineInternal;

    class GOLDEN_SUN_API PointLight final
    {
        friend class EngineInternal;

        DISALLOW_COPY_AND_ASSIGN(PointLight)

    public:
        PointLight();
        ~PointLight() noexcept;

        PointLight(PointLight&& other) noexcept;
        PointLight& operator=(PointLight&& other) noexcept;

        PointLight Clone() const;

        DirectX::XMFLOAT3& Position() noexcept;
        DirectX::XMFLOAT3 const& Position() const noexcept;

        DirectX::XMFLOAT3& Color() noexcept;
        DirectX::XMFLOAT3 const& Color() const noexcept;

        DirectX::XMFLOAT3& Falloff() noexcept;
        DirectX::XMFLOAT3 const& Falloff() const noexcept;

        bool& Shadowing() noexcept;
        bool Shadowing() const noexcept;

    private:
        class Impl;
        Impl* impl_;
    };
} // namespace GoldenSun
