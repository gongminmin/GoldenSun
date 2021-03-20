#pragma once

#include <memory>

#include <DirectXMath.h>
#include <d3d12.h>
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
        DISALLOW_COPY_AND_ASSIGN(Engine);

    public:
        explicit Engine(ID3D12Device5* device, ID3D12CommandQueue* cmd_queue);
        ~Engine() noexcept;

        Engine(Engine&& other) noexcept;
        Engine& operator=(Engine&& other) noexcept;

        void RenderTarget(uint32_t width, uint32_t height, DXGI_FORMAT format);
        void Geometries(ID3D12GraphicsCommandList4* cmd_list, Mesh const* meshes, uint32_t num_meshes);
        void Camera(DirectX::XMFLOAT3 const& eye, DirectX::XMFLOAT3 const& look_at, DirectX::XMFLOAT3 const& up, float fov,
            float near_plane, float far_plane);
        void Light(DirectX::XMFLOAT3 const& pos, DirectX::XMFLOAT3 const& color, DirectX::XMFLOAT3 const& falloff);

        void Render(ID3D12GraphicsCommandList4* cmd_list);

        ID3D12Resource* Output() const noexcept;

    private:
        class EngineImpl;
        EngineImpl* impl_;
    };
} // namespace GoldenSun
