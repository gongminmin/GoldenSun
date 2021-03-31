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

        void Position(DirectX::XMFLOAT3 const& value) noexcept;
        DirectX::XMFLOAT3 const& Position() const noexcept;

        void Color(DirectX::XMFLOAT3 const& value) noexcept;
        DirectX::XMFLOAT3 const& Color() const noexcept;

        void Falloff(DirectX::XMFLOAT3 const& value) noexcept;
        DirectX::XMFLOAT3 const& Falloff() const noexcept;

        void Shadowing(bool value) noexcept;
        bool Shadowing() const noexcept;

    private:
        class Impl;
        Impl* impl_;
    };
} // namespace GoldenSun
