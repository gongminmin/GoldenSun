#include "pch.hpp"

#include <GoldenSun/Engine.hpp>

using namespace DirectX;

namespace GoldenSun
{
    class Mesh::Impl
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl(DXGI_FORMAT vertex_fmt, uint32_t vb_stride_in_bytes, DXGI_FORMAT index_fmt, uint32_t ib_stride_in_bytes)
            : vertex_format_(vertex_fmt), vertex_stride_in_bytes_(vb_stride_in_bytes), index_format_(index_fmt),
              index_stride_in_bytes_(ib_stride_in_bytes)
        {
        }

        DXGI_FORMAT VertexFormat() const noexcept
        {
            return vertex_format_;
        }
        uint32_t VertexStrideInBytes() const noexcept
        {
            return vertex_stride_in_bytes_;
        }

        DXGI_FORMAT IndexFormat() const noexcept
        {
            return index_format_;
        }
        uint32_t IndexStrideInBytes() const noexcept
        {
            return index_stride_in_bytes_;
        }

        uint32_t AddMaterial(PbrMaterial const& material)
        {
            uint32_t const material_id = static_cast<uint32_t>(materials_.size());
            materials_.push_back(material);
            return material_id;
        }

        uint32_t NumMaterials() const noexcept
        {
            return static_cast<uint32_t>(materials_.size());
        }

        PbrMaterial const& Material(uint32_t material_id) const noexcept
        {
            return materials_[material_id];
        }

        uint32_t AddPrimitive(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id, D3D12_RAYTRACING_GEOMETRY_FLAGS flags)
        {
            uint32_t const primitive_id = static_cast<uint32_t>(primitives_.size());

            auto& new_primitive = primitives_.emplace_back();

            new_primitive.vb.resource = vb;
            new_primitive.vb.count = static_cast<uint32_t>(vb->GetDesc().Width / vertex_stride_in_bytes_);
            new_primitive.vb.vertex_buffer.StrideInBytes = vertex_stride_in_bytes_;
            new_primitive.vb.vertex_buffer.StartAddress = vb->GetGPUVirtualAddress();

            new_primitive.ib.resource = ib;
            new_primitive.ib.count = static_cast<uint32_t>(ib->GetDesc().Width / index_stride_in_bytes_);
            new_primitive.ib.index_buffer = ib->GetGPUVirtualAddress();

            assert(new_primitive.material_id < this->NumMaterials());

            new_primitive.material_id = material_id;
            new_primitive.flags = flags;

            return primitive_id;
        }

        uint32_t NumPrimitives() const noexcept
        {
            return static_cast<uint32_t>(primitives_.size());
        }

        uint32_t NumVertices(uint32_t primitive_id) const noexcept
        {
            return primitives_[primitive_id].vb.count;
        }
        ID3D12Resource* VertexBuffer(uint32_t primitive_id) const noexcept
        {
            return primitives_[primitive_id].vb.resource.Get();
        }

        uint32_t NumIndices(uint32_t primitive_id) const noexcept
        {
            return primitives_[primitive_id].ib.count;
        }
        ID3D12Resource* IndexBuffer(uint32_t primitive_id) const noexcept
        {
            return primitives_[primitive_id].ib.resource.Get();
        }

        uint32_t MaterialId(uint32_t primitive_id) const noexcept
        {
            return primitives_[primitive_id].material_id;
        }

        uint32_t AddInstance(XMFLOAT4X4 const& transform)
        {
            uint32_t const instance_id = static_cast<uint32_t>(instances_.size());

            auto& new_instance = instances_.emplace_back();
            new_instance.transform = transform;

            return instance_id;
        }

        uint32_t NumInstances() const noexcept
        {
            return static_cast<uint32_t>(instances_.size());
        }

        XMFLOAT4X4 const& Transform(uint32_t instance_id) const noexcept
        {
            return instances_[instance_id].transform;
        }

        void Transform(uint32_t instance_id, XMFLOAT4X4 const& transform) noexcept
        {
            instances_[instance_id].transform = transform;
        }

        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> GeometryDescs() const
        {
            std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> ret;
            ret.reserve(primitives_.size());

            for (auto const& primitive : primitives_)
            {
                auto& geometry_desc = ret.emplace_back();
                geometry_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
                geometry_desc.Flags = primitive.flags;
                geometry_desc.Triangles.Transform3x4 = 0; // TODO: Support per-primitive transform
                geometry_desc.Triangles.VertexBuffer = primitive.vb.vertex_buffer;
                geometry_desc.Triangles.VertexCount = primitive.vb.count;
                geometry_desc.Triangles.VertexFormat = vertex_format_;
                geometry_desc.Triangles.IndexBuffer = primitive.ib.index_buffer;
                geometry_desc.Triangles.IndexCount = primitive.ib.count;
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

        struct Primitive
        {
            Buffer vb;
            Buffer ib;
            uint32_t material_id;
            D3D12_RAYTRACING_GEOMETRY_FLAGS flags;
        };

        struct Instance
        {
            XMFLOAT4X4 transform;
        };

    private:
        std::vector<PbrMaterial> materials_;
        std::vector<Primitive> primitives_;
        std::vector<Instance> instances_;
        DXGI_FORMAT vertex_format_;
        uint32_t vertex_stride_in_bytes_;
        DXGI_FORMAT index_format_;
        uint32_t index_stride_in_bytes_;
    };


    Mesh::Mesh(DXGI_FORMAT vertex_fmt, uint32_t vb_stride_in_bytes, DXGI_FORMAT index_fmt, uint32_t ib_stride_in_bytes)
        : impl_(new Impl(vertex_fmt, vb_stride_in_bytes, index_fmt, ib_stride_in_bytes))
    {
    }

    Mesh::~Mesh() noexcept
    {
        delete impl_;
        impl_ = nullptr;
    }

    Mesh::Mesh(Mesh&& other) noexcept : impl_(std::move(other.impl_))
    {
        other.impl_ = nullptr;
    }

    Mesh& Mesh::operator=(Mesh&& other) noexcept
    {
        if (this != &other)
        {
            impl_ = std::move(other.impl_);
            other.impl_ = nullptr;
        }
        return *this;
    }

    DXGI_FORMAT Mesh::VertexFormat() const noexcept
    {
        return impl_->VertexFormat();
    }

    uint32_t Mesh::VertexStrideInBytes() const noexcept
    {
        return impl_->VertexStrideInBytes();
    }

    DXGI_FORMAT Mesh::IndexFormat() const noexcept
    {
        return impl_->IndexFormat();
    }

    uint32_t Mesh::IndexStrideInBytes() const noexcept
    {
        return impl_->IndexStrideInBytes();
    }

    uint32_t Mesh::AddMaterial(PbrMaterial const& material)
    {
        return impl_->AddMaterial(material);
    }

    uint32_t Mesh::NumMaterials() const noexcept
    {
        return impl_->NumMaterials();
    }

    PbrMaterial const& Mesh::Material(uint32_t material_id) const noexcept
    {
        return impl_->Material(material_id);
    }

    uint32_t Mesh::AddPrimitive(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id)
    {
        return impl_->AddPrimitive(vb, ib, material_id, D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE);
    }

    uint32_t Mesh::AddPrimitive(ID3D12Resource* vb, ID3D12Resource* ib, uint32_t material_id, D3D12_RAYTRACING_GEOMETRY_FLAGS flags)
    {
        return impl_->AddPrimitive(vb, ib, material_id, flags);
    }

    uint32_t Mesh::NumPrimitives() const noexcept
    {
        return impl_->NumPrimitives();
    }

    uint32_t Mesh::NumVertices(uint32_t primitive_id) const noexcept
    {
        return impl_->NumVertices(primitive_id);
    }

    ID3D12Resource* Mesh::VertexBuffer(uint32_t primitive_id) const noexcept
    {
        return impl_->VertexBuffer(primitive_id);
    }

    uint32_t Mesh::NumIndices(uint32_t primitive_id) const noexcept
    {
        return impl_->NumIndices(primitive_id);
    }

    ID3D12Resource* Mesh::IndexBuffer(uint32_t primitive_id) const noexcept
    {
        return impl_->IndexBuffer(primitive_id);
    }

    uint32_t Mesh::MaterialId(uint32_t primitive_id) const noexcept
    {
        return impl_->MaterialId(primitive_id);
    }

    uint32_t Mesh::AddInstance(XMFLOAT4X4 const& transform)
    {
        return impl_->AddInstance(transform);
    }

    uint32_t Mesh::NumInstances() const noexcept
    {
        return impl_->NumInstances();
    }

    XMFLOAT4X4 const& Mesh::Transform(uint32_t instance_id) const noexcept
    {
        return impl_->Transform(instance_id);
    }

    void Mesh::Transform(uint32_t instance_id, XMFLOAT4X4 const& transform) noexcept
    {
        impl_->Transform(instance_id, transform);
    }

    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> Mesh::GeometryDescs() const
    {
        return impl_->GeometryDescs();
    }
} // namespace GoldenSun
