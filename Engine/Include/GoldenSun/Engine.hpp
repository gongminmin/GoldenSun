#pragma once

#include <memory>

#include <DirectXMath.h>
#include <dxgiformat.h>

#include <GoldenSun/Base.hpp>
#include <GoldenSun/Mesh.hpp>

struct ID3D12CommandQueue;
struct ID3D12GraphicsCommandList4;
struct ID3D12Resource;

namespace GoldenSun
{
    class GOLDEN_SUN_API Engine
    {
    public:
        virtual ~Engine();

        virtual void RenderTarget(uint32_t width, uint32_t height, DXGI_FORMAT format) = 0;
        virtual void Geometry(std::unique_ptr<Mesh> const* meshes, uint32_t num_meshes, ID3D12Resource* mb, uint32_t num_materials) = 0;
        virtual void Camera(DirectX::XMFLOAT3 const& eye, DirectX::XMFLOAT3 const& look_at, DirectX::XMFLOAT3 const& up, float fov,
            float near_plane, float far_plane) = 0;
        virtual void Light(DirectX::XMFLOAT3 const& pos, DirectX::XMFLOAT3 const& color) = 0;

        virtual void Render(ID3D12GraphicsCommandList4* cmd_list) = 0;

        virtual ID3D12Resource* Output() const noexcept = 0;
    };

    GOLDEN_SUN_API std::unique_ptr<Engine> CreateEngineD3D12(ID3D12CommandQueue* cmd_queue);
} // namespace GoldenSun
