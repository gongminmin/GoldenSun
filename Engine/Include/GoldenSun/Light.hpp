#pragma once

#include <memory>
#include <vector>

#include <DirectXMath.h>
#include <dxgiformat.h>

namespace GoldenSun
{
    struct Light
    {
        // Must match Light in shader
        struct Buffer
        {
            alignas(4) DirectX::XMFLOAT3 position;
            alignas(4) DirectX::XMFLOAT3 color;
            alignas(4) DirectX::XMFLOAT3 falloff;
            alignas(4) bool shadowing;
            alignas(4) uint8_t paddings[24]{};
        };
        static_assert(sizeof(Buffer) % 16 == 0);
        static_assert(sizeof(Buffer) % 64 == 0); // To align with GpuMemoryBlock, for now

        Buffer buffer;
    };
} // namespace GoldenSun
