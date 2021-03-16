#include "pch.hpp"

#include <GoldenSun/Engine.hpp>

using namespace GoldenSun;

namespace
{
    class MeshD3D12 : public Mesh
    {
    public:
        MeshD3D12(DXGI_FORMAT vertex_fmt, uint32_t vb_stride_in_bytes, DXGI_FORMAT index_fmt, uint32_t ib_stride_in_bytes)
            : vertex_format_(vertex_fmt), vertex_stride_in_bytes_(vb_stride_in_bytes), index_format_(index_fmt),
              index_stride_in_bytes_(ib_stride_in_bytes)
        {
        }

        void AddGeometry(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id) override
        {
            this->AddGeometry(vb, ib, material_id, D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE);
        }

        void AddGeometry(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id, D3D12_RAYTRACING_GEOMETRY_FLAGS flags) override
        {
            auto& new_geometry = geometries_.emplace_back();

            new_geometry.vb.resource = vb;
            new_geometry.vb.count = static_cast<uint32_t>(vb->GetDesc().Width / vertex_stride_in_bytes_);
            new_geometry.vb.vertex_buffer.StrideInBytes = vertex_stride_in_bytes_;
            new_geometry.vb.vertex_buffer.StartAddress = vb->GetGPUVirtualAddress();

            new_geometry.ib.resource = ib;
            new_geometry.ib.count = static_cast<uint32_t>(ib->GetDesc().Width / index_stride_in_bytes_);
            new_geometry.ib.index_buffer = ib->GetGPUVirtualAddress();

            new_geometry.material_id = material_id;
            new_geometry.flags = flags;
        }

        DXGI_FORMAT VertexFormat() const noexcept override
        {
            return vertex_format_;
        }
        uint32_t VertexStrideInBytes() const noexcept override
        {
            return vertex_stride_in_bytes_;
        }
        uint32_t NumVertices(uint32_t index) const noexcept override
        {
            return geometries_[index].vb.count;
        }
        ID3D12Resource* VertexBuffer(uint32_t index) const noexcept override
        {
            return geometries_[index].vb.resource.Get();
        }

        DXGI_FORMAT IndexFormat() const noexcept override
        {
            return index_format_;
        }
        uint32_t IndexStrideInBytes() const noexcept override
        {
            return index_stride_in_bytes_;
        }
        uint32_t NumIndices(uint32_t index) const noexcept override
        {
            return geometries_[index].ib.count;
        }
        ID3D12Resource* IndexBuffer(uint32_t index) const noexcept override
        {
            return geometries_[index].ib.resource.Get();
        }

        uint32_t MaterialId(uint32_t index) const noexcept
        {
            return geometries_[index].material_id;
        }

        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> GeometryDescs() const override
        {
            std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> ret;
            ret.reserve(geometries_.size());

            for (auto const& geometry : geometries_)
            {
                auto& geometry_desc = ret.emplace_back();
                geometry_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
                geometry_desc.Flags = geometry.flags;
                geometry_desc.Triangles.Transform3x4 = 0;
                geometry_desc.Triangles.VertexBuffer = geometry.vb.vertex_buffer;
                geometry_desc.Triangles.VertexCount = geometry.vb.count;
                geometry_desc.Triangles.VertexFormat = vertex_format_;
                geometry_desc.Triangles.IndexBuffer = geometry.ib.index_buffer;
                geometry_desc.Triangles.IndexCount = geometry.ib.count;
                geometry_desc.Triangles.IndexFormat = index_format_;
            }

            return ret;
        }

    private:
        struct Buffer
        {
            ComPtr<ID3D12Resource> resource;
            uint32_t count;
            union
            {
                D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE vertex_buffer;
                D3D12_GPU_VIRTUAL_ADDRESS index_buffer;
            };
        };

        struct Geometry
        {
            Buffer vb;
            Buffer ib;
            uint32_t material_id;
            D3D12_RAYTRACING_GEOMETRY_FLAGS flags;
        };

    private:
        std::vector<Geometry> geometries_;
        DXGI_FORMAT vertex_format_;
        uint32_t vertex_stride_in_bytes_;
        DXGI_FORMAT index_format_;
        uint32_t index_stride_in_bytes_;
    };
} // namespace

namespace GoldenSun
{
    Mesh::~Mesh() = default;

    std::unique_ptr<Mesh> CreateMeshD3D12(
        DXGI_FORMAT vertex_fmt, uint32_t vb_stride_in_bytes, DXGI_FORMAT index_fmt, uint32_t ib_stride_in_bytes)
    {
        return std::make_unique<MeshD3D12>(vertex_fmt, vb_stride_in_bytes, index_fmt, ib_stride_in_bytes);
    }
} // namespace GoldenSun
