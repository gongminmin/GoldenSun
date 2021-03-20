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

    struct PbrMaterial
    {
        // Must match PbrMaterial in shader
        struct Buffer
        {
            alignas(4) DirectX::XMFLOAT3 albedo{0, 0, 0};
            alignas(4) float opacity{1};
            alignas(4) DirectX::XMFLOAT3 emissive{0, 0, 0};
            alignas(4) float metalness{0};
            alignas(4) float glossiness{1};
            alignas(4) float alpha_test{0};
            alignas(4) float normal_scale{1};
            alignas(4) float occlusion_strength{1};
            alignas(4) bool transparent{false};
            alignas(4) bool two_sided{false};
            alignas(4) uint8_t paddings[8]{};
        };
        static_assert(sizeof(Buffer) % 16 == 0);

        Buffer buffer;

        enum class TextureSlot
        {
            Albedo,
            MetalnessGlossiness,
            Emissive,
            Normal,
            Height,
            Occlusion,

            Num
        };

        std::array<std::string, ConvertToUint(TextureSlot::Num)> textures;
    };

    class GOLDEN_SUN_API Mesh
    {
        DISALLOW_COPY_AND_ASSIGN(Mesh);

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
        PbrMaterial const& Material(uint32_t material_id) const noexcept;

        // TODO: Support adding a region of buffers as a primitive
        uint32_t AddPrimitive(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id);
        uint32_t AddPrimitive(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id, D3D12_RAYTRACING_GEOMETRY_FLAGS flags);

        uint32_t NumPrimitives() const noexcept;

        uint32_t NumVertices(uint32_t primitive_id) const noexcept;
        ID3D12Resource* VertexBuffer(uint32_t primitive_id) const noexcept;
        uint32_t NumIndices(uint32_t primitive_id) const noexcept;
        ID3D12Resource* IndexBuffer(uint32_t primitive_id) const noexcept;

        uint32_t MaterialId(uint32_t primitive_id) const noexcept;

        uint32_t AddInstance(DirectX::XMFLOAT4X4 const& transform);
        uint32_t NumInstances() const noexcept;
        DirectX::XMFLOAT4X4 const& Transform(uint32_t instance_id) const noexcept;

        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> GeometryDescs() const;

    private:
        class MeshImpl;
        MeshImpl* impl_;
    };
} // namespace GoldenSun
