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
        DISALLOW_COPY_AND_ASSIGN(Mesh);

    public:
        Mesh(DXGI_FORMAT vertex_fmt, uint32_t vb_stride_in_bytes, DXGI_FORMAT index_fmt, uint32_t ib_stride_in_bytes);
        ~Mesh() noexcept;

        Mesh(Mesh&& other) noexcept;
        Mesh& operator=(Mesh&& other) noexcept;

        // TODO: Support adding a region of buffers as a primitive
        uint32_t AddPrimitive(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id);
        uint32_t AddPrimitive(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id, D3D12_RAYTRACING_GEOMETRY_FLAGS flags);

        DXGI_FORMAT VertexFormat() const noexcept;
        uint32_t VertexStrideInBytes() const noexcept;
        uint32_t NumVertices(uint32_t primitive_id) const noexcept;
        ID3D12Resource* VertexBuffer(uint32_t primitive_id) const noexcept;

        DXGI_FORMAT IndexFormat() const noexcept;
        uint32_t IndexStrideInBytes() const noexcept;
        uint32_t NumIndices(uint32_t primitive_id) const noexcept;
        ID3D12Resource* IndexBuffer(uint32_t primitive_id) const noexcept;

        uint32_t MaterialId(uint32_t primitive_id) const noexcept;

        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> GeometryDescs() const;

    private:
        class MeshImpl;
        MeshImpl* impl_;
    };
} // namespace GoldenSun
