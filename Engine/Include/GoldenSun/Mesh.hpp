#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgiformat.h>

struct ID3D12Resource;

namespace GoldenSun
{
    class EngineInternal;
    class PbrMaterial;

    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 tangent_quat;
        DirectX::XMFLOAT2 tex_coord;
    };

    using Index = uint16_t;

    struct MeshInstance
    {
        DirectX::XMFLOAT4X4 transform;
    };

    class GOLDEN_SUN_API Mesh final
    {
        friend class EngineInternal;

        DISALLOW_COPY_AND_ASSIGN(Mesh)

    public:
        Mesh(DXGI_FORMAT vertex_fmt, uint32_t vb_stride_in_bytes, DXGI_FORMAT index_fmt, uint32_t ib_stride_in_bytes);
        ~Mesh() noexcept;

        Mesh(Mesh&& other) noexcept;
        Mesh& operator=(Mesh&& other) noexcept;

        DXGI_FORMAT VertexFormat() const noexcept;
        uint32_t VertexStrideInBytes() const noexcept;

        DXGI_FORMAT IndexFormat() const noexcept;
        uint32_t IndexStrideInBytes() const noexcept;

        uint32_t AddMaterial(PbrMaterial const& material);
        uint32_t NumMaterials() const noexcept;
        PbrMaterial& Material(uint32_t material_id) noexcept;
        PbrMaterial const& Material(uint32_t material_id) const noexcept;

        // TODO: Support adding a region of buffers as a primitive
        uint32_t AddPrimitive(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id);
        uint32_t AddPrimitive(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id, D3D12_RAYTRACING_GEOMETRY_FLAGS flags);

        uint32_t NumPrimitives() const noexcept;

        uint32_t NumVertices(uint32_t primitive_id) const noexcept;
        ID3D12Resource* VertexBuffer(uint32_t primitive_id) const noexcept;
        uint32_t NumIndices(uint32_t primitive_id) const noexcept;
        ID3D12Resource* IndexBuffer(uint32_t primitive_id) const noexcept;

        void MaterialId(uint32_t primitive_id, uint32_t id) noexcept;
        uint32_t MaterialId(uint32_t primitive_id) const noexcept;

        uint32_t AddInstance(MeshInstance const& instance);
        uint32_t NumInstances() const noexcept;
        MeshInstance& Instance(uint32_t instance_id) noexcept;
        MeshInstance const& Instance(uint32_t instance_id) const noexcept;

    private:
        class Impl;
        Impl* impl_;
    };
} // namespace GoldenSun
