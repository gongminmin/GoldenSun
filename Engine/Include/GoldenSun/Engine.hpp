#pragma once

#include <DirectXMath.h>
#include <dxgiformat.h>

struct ID3D12Device5;
struct ID3D12CommandQueue;
struct ID3D12GraphicsCommandList4;
struct ID3D12Resource;

namespace GoldenSun
{
    class Mesh;
    class PointLight;

    class GOLDEN_SUN_API Engine final
    {
        DISALLOW_COPY_AND_ASSIGN(Engine)

    public:
        Engine();
        Engine(ID3D12Device5* device, ID3D12CommandQueue* cmd_queue);
        ~Engine() noexcept;

        Engine(Engine&& other) noexcept;
        Engine& operator=(Engine&& other) noexcept;

        void RenderTarget(uint32_t width, uint32_t height, DXGI_FORMAT format);
        void Meshes(Mesh const* meshes, uint32_t num_meshes);
        void Lights(PointLight const* lights, uint32_t num_lights);
        void Camera(Camera const& camera);

        void Render(ID3D12GraphicsCommandList4* cmd_list);

        ID3D12Resource* Output() const noexcept;

    private:
        class Impl;
        Impl* impl_;
    };
} // namespace GoldenSun
