#pragma once

#include <memory>
#include <vector>

#include <DirectXMath.h>
#include <dxgiformat.h>

#include <GoldenSun/Base.hpp>

struct ID3D12CommandQueue;
struct ID3D12GraphicsCommandList4;
struct ID3D12Resource;

namespace GoldenSun
{
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
    };

    using Index = uint16_t;

    struct Material
    {
        DirectX::XMFLOAT4 albedo;
    };

    class GOLDEN_SUN_API Mesh
    {
    public:
        virtual ~Mesh();

        virtual void AddGeometry(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id) = 0;
        virtual void AddGeometry(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id, D3D12_RAYTRACING_GEOMETRY_FLAGS flags) = 0;

        virtual DXGI_FORMAT VertexFormat() const noexcept = 0;
        virtual uint32_t VertexStrideInBytes() const noexcept = 0;
        virtual uint32_t NumVertices(uint32_t index) const noexcept = 0;
        virtual ID3D12Resource* VertexBuffer(uint32_t index) const noexcept = 0;

        virtual DXGI_FORMAT IndexFormat() const noexcept = 0;
        virtual uint32_t IndexStrideInBytes() const noexcept = 0;
        virtual uint32_t NumIndices(uint32_t index) const noexcept = 0;
        virtual ID3D12Resource* IndexBuffer(uint32_t index) const noexcept = 0;

        virtual uint32_t MaterialId(uint32_t index) const noexcept = 0;

        virtual std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> GeometryDescs() const = 0;
    };

    GOLDEN_SUN_API std::unique_ptr<Mesh> CreateMeshD3D12(
        DXGI_FORMAT vertex_fmt, uint32_t vb_stride_in_bytes, DXGI_FORMAT index_fmt, uint32_t ib_stride_in_bytes);
} // namespace GoldenSun
