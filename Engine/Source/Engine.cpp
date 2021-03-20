#include "pch.hpp"

#include <GoldenSun/Engine.hpp>
#include <GoldenSun/SmartPtrHelper.hpp>
#include <GoldenSun/Util.hpp>

#include <cassert>
#include <iomanip>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include "AccelerationStructure.hpp"

#include "CompiledShaders/RayTracing.h"

using namespace DirectX;
using namespace GoldenSun;

DEFINE_UUID_OF(ID3D12CommandAllocator);
DEFINE_UUID_OF(ID3D12DescriptorHeap);
DEFINE_UUID_OF(ID3D12Device5);
DEFINE_UUID_OF(ID3D12Fence);
DEFINE_UUID_OF(ID3D12GraphicsCommandList4);
DEFINE_UUID_OF(ID3D12Resource);
DEFINE_UUID_OF(ID3D12RootSignature);
DEFINE_UUID_OF(ID3D12StateObject);
DEFINE_UUID_OF(ID3D12StateObjectProperties);

namespace
{
    struct RayPayload
    {
        XMFLOAT4 color;
    };
    static_assert(sizeof(RayPayload) == sizeof(XMFLOAT4));

    struct RayAttrib
    {
        XMFLOAT2 barycentrics;
    };
    static_assert(sizeof(RayAttrib) == sizeof(XMFLOAT2));

    // Shader record = {{Shader ID}, {RootArguments}}
    class ShaderRecord
    {
    public:
        ShaderRecord(void* shader_identifier, uint32_t shader_identifier_size) noexcept
            : ShaderRecord(shader_identifier, shader_identifier_size, nullptr, 0)
        {
        }

        ShaderRecord(void* shader_identifier, uint32_t shader_identifier_size, void* local_root_arguments,
            uint32_t local_root_arguments_size) noexcept
            : shader_identifier{shader_identifier, shader_identifier_size}, local_root_arguments{
                                                                                local_root_arguments, local_root_arguments_size}
        {
        }

        void CopyTo(void* dest) const
        {
            uint8_t* u8_dest = static_cast<uint8_t*>(dest);
            memcpy(u8_dest, shader_identifier.ptr, shader_identifier.size);
            if (local_root_arguments.size != 0)
            {
                memcpy(u8_dest + shader_identifier.size, local_root_arguments.ptr, local_root_arguments.size);
            }
        }

        struct PointerWithSize
        {
            void* ptr = nullptr;
            uint32_t size = 0;
        };
        PointerWithSize shader_identifier;
        PointerWithSize local_root_arguments;
    };

    // Shader table = {{ShaderRecord 1}, {ShaderRecord 2}, ...}
    class ShaderTable
    {
    public:
        ShaderTable(ID3D12Device5* device, uint32_t num_shader_records, uint32_t shader_record_size)
            : ShaderTable(device, num_shader_records, shader_record_size, nullptr)
        {
        }

        ShaderTable(ID3D12Device5* device, uint32_t max_num_shader_records, uint32_t shader_record_size, wchar_t const* resource_name)
            : shader_record_size_(Align<D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT>(shader_record_size)),
              buffer_(device, max_num_shader_records * shader_record_size_, resource_name)
        {
            mapped_shader_records_ = buffer_.MappedData<uint8_t>();
        }

        ~ShaderTable() noexcept
        {
        }

        ID3D12Resource* Resource() const noexcept
        {
            return buffer_.Resource();
        }

        void push_back(ShaderRecord const& shader_record)
        {
            shader_record.CopyTo(mapped_shader_records_);
            mapped_shader_records_ += shader_record_size_;
            ++num_shader_records_;
        }

        uint32_t ShaderRecordSize() const noexcept
        {
            return shader_record_size_;
        }

        uint32_t NumShaderRecords() const noexcept
        {
            return num_shader_records_;
        }

    private:
        uint32_t shader_record_size_;
        uint32_t num_shader_records_ = 0;

        GpuUploadBuffer buffer_;
        uint8_t* mapped_shader_records_;
    };

    class D3D12StateSubObject
    {
    public:
        virtual ~D3D12StateSubObject() = default;

        D3D12_STATE_SUBOBJECT const& Get() const noexcept
        {
            return *subobject_;
        }

    protected:
        D3D12_STATE_SUBOBJECT const* subobject_;
    };

    class D3D12StateObjectDescHelper final
    {
        friend class D3D12DxilLibrarySubObject;
        friend class D3D12SubobjectToExportsAssociationSubobject;
        friend class D3D12HitGroupSubobject;
        friend class D3D12RayTracingShaderConfigSubobject;
        friend class D3D12RayTracingPipelineConfigSubobject;
        friend class D3D12GlobalRootSignatureSubobject;
        friend class D3D12LocalRootSignatureSubobject;

    public:
        D3D12StateObjectDescHelper() noexcept : D3D12StateObjectDescHelper(D3D12_STATE_OBJECT_TYPE_COLLECTION)
        {
        }

        explicit D3D12StateObjectDescHelper(D3D12_STATE_OBJECT_TYPE type) noexcept
        {
            desc_.Type = type;
            desc_.pSubobjects = nullptr;
            desc_.NumSubobjects = 0;
        }

        D3D12StateObjectDescHelper(D3D12StateObjectDescHelper const& rhs) = delete;
        D3D12StateObjectDescHelper& operator=(D3D12StateObjectDescHelper const& rhs) = delete;

        D3D12_STATE_OBJECT_DESC const& Retrieve()
        {
            repointed_associations_.clear();
            subobject_array_.clear();
            subobject_array_.reserve(desc_.NumSubobjects);
            for (auto iter = subobject_list_.begin(); iter != subobject_list_.end(); ++iter)
            {
                subobject_array_.push_back(*iter);
                iter->subobject_array_location = &subobject_array_.back();
            }
            for (uint32_t i = 0; i < desc_.NumSubobjects; ++i)
            {
                if (subobject_array_[i].Type == D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION)
                {
                    auto const& original_subobject_association =
                        *static_cast<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION const*>(subobject_array_[i].pDesc);
                    auto repointed = original_subobject_association;
                    auto const& wrapper = *static_cast<SubobjectWrapper const*>(original_subobject_association.pSubobjectToAssociate);
                    repointed.pSubobjectToAssociate = wrapper.subobject_array_location;
                    repointed_associations_.emplace_back(std::move(repointed));
                    subobject_array_[i].pDesc = &repointed_associations_.back();
                }
            }

            desc_.pSubobjects = desc_.NumSubobjects ? subobject_array_.data() : nullptr;
            return desc_;
        }

        template <typename T>
        T& CreateSubobject()
        {
            auto subobject = std::make_unique<T>(*this);
            auto* subobject_raw = subobject.get();
            owned_subobjects_.emplace_back(std::move(subobject));
            return *subobject_raw;
        }

    private:
        struct SubobjectWrapper final : public D3D12_STATE_SUBOBJECT
        {
            D3D12_STATE_SUBOBJECT* subobject_array_location;
        };

        D3D12_STATE_SUBOBJECT& CreateSubobject(D3D12_STATE_SUBOBJECT_TYPE type, void* desc)
        {
            SubobjectWrapper subobject;
            subobject.Type = type;
            subobject.pDesc = desc;
            subobject.subobject_array_location = nullptr;
            subobject_list_.emplace_back(std::move(subobject));
            ++desc_.NumSubobjects;
            return subobject_list_.back();
        }

    private:
        D3D12_STATE_OBJECT_DESC desc_;

        std::list<SubobjectWrapper> subobject_list_;
        std::vector<D3D12_STATE_SUBOBJECT> subobject_array_;
        std::list<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> repointed_associations_;

        std::list<std::unique_ptr<D3D12StateSubObject>> owned_subobjects_;
    };

    class D3D12DxilLibrarySubObject final : public D3D12StateSubObject
    {
    public:
        explicit D3D12DxilLibrarySubObject(D3D12StateObjectDescHelper& parent)
        {
            subobject_ = &parent.CreateSubobject(D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &desc_);
        }

        void SetDxilLibrary(D3D12_SHADER_BYTECODE const* code) noexcept
        {
            desc_.DXILLibrary = code ? *code : D3D12_SHADER_BYTECODE{};
        }

        void DefineExport(
            std::wstring_view name, std::wstring_view export_to_rename = {}, D3D12_EXPORT_FLAGS flags = D3D12_EXPORT_FLAG_NONE)
        {
            D3D12_EXPORT_DESC export_desc = {
                name.empty() ? nullptr : string_table_.emplace_back(std::move(name)).c_str(),
                export_to_rename.empty() ? nullptr : string_table_.emplace_back(std::move(export_to_rename)).c_str(),
                flags,
            };
            exports_.emplace_back(std::move(export_desc));

            desc_.pExports = exports_.data();
            desc_.NumExports = static_cast<uint32_t>(exports_.size());
        }

    private:
        D3D12_DXIL_LIBRARY_DESC desc_{};

        std::list<std::wstring> string_table_;
        std::vector<D3D12_EXPORT_DESC> exports_;
    };

    class D3D12SubobjectToExportsAssociationSubobject final : public D3D12StateSubObject
    {
    public:
        explicit D3D12SubobjectToExportsAssociationSubobject(D3D12StateObjectDescHelper& parent)
        {
            subobject_ = &parent.CreateSubobject(D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &association_);
        }

        void SetSubobjectToAssociate(D3D12_STATE_SUBOBJECT const& subobject_to_associate) noexcept
        {
            association_.pSubobjectToAssociate = &subobject_to_associate;
        }

        void AddExport(std::wstring_view export_name)
        {
            ++association_.NumExports;
            exports_.emplace_back(export_name.empty() ? nullptr : string_table_.emplace_back(std::move(export_name)).c_str());
            association_.pExports = exports_.data();
        }

    private:
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association_{};

        std::list<std::wstring> string_table_;
        std::vector<wchar_t const*> exports_;
    };

    class D3D12HitGroupSubobject final : public D3D12StateSubObject
    {
    public:
        explicit D3D12HitGroupSubobject(D3D12StateObjectDescHelper& parent)
        {
            subobject_ = &parent.CreateSubobject(D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &desc_);
        }

        void SetHitGroupExport(std::wstring_view export_name)
        {
            string_table_[0] = std::wstring(std::move(export_name));
            desc_.HitGroupExport = string_table_[0].empty() ? nullptr : string_table_[0].c_str();
        }

        void SetHitGroupType(D3D12_HIT_GROUP_TYPE type) noexcept
        {
            desc_.Type = type;
        }

        void SetAnyHitShaderImport(std::wstring_view import_name)
        {
            string_table_[1] = std::wstring(std::move(import_name));
            desc_.AnyHitShaderImport = string_table_[1].empty() ? nullptr : string_table_[1].c_str();
        }

        void SetClosestHitShaderImport(std::wstring_view import_name)
        {
            string_table_[2] = std::wstring(std::move(import_name));
            desc_.ClosestHitShaderImport = string_table_[2].empty() ? nullptr : string_table_[2].c_str();
        }

        void SetIntersectionShaderImport(std::wstring_view import_name)
        {
            string_table_[3] = std::wstring(std::move(import_name));
            desc_.IntersectionShaderImport = string_table_[3].empty() ? nullptr : string_table_[3].c_str();
        }

    private:
        D3D12_HIT_GROUP_DESC desc_{};

        std::wstring string_table_[4];
    };

    class D3D12RayTracingShaderConfigSubobject final : public D3D12StateSubObject
    {
    public:
        explicit D3D12RayTracingShaderConfigSubobject(D3D12StateObjectDescHelper& parent)
        {
            subobject_ = &parent.CreateSubobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &config_);
        }

        void Config(uint32_t max_payload_size, uint32_t max_attrib_size) noexcept
        {
            config_.MaxPayloadSizeInBytes = max_payload_size;
            config_.MaxAttributeSizeInBytes = max_attrib_size;
        }

    private:
        D3D12_RAYTRACING_SHADER_CONFIG config_{};
    };

    class D3D12RayTracingPipelineConfigSubobject final : public D3D12StateSubObject
    {
    public:
        explicit D3D12RayTracingPipelineConfigSubobject(D3D12StateObjectDescHelper& parent)
        {
            subobject_ = &parent.CreateSubobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &config_);
        }

        void Config(uint32_t max_trace_recursion_depth) noexcept
        {
            config_.MaxTraceRecursionDepth = max_trace_recursion_depth;
        }

    private:
        D3D12_RAYTRACING_PIPELINE_CONFIG config_{};
    };

    class D3D12GlobalRootSignatureSubobject final : public D3D12StateSubObject
    {
    public:
        explicit D3D12GlobalRootSignatureSubobject(D3D12StateObjectDescHelper& parent)
        {
            subobject_ = &parent.CreateSubobject(D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, root_sig_.PutVoid());
        }

        void SetRootSignature(ID3D12RootSignature* root_sig) noexcept
        {
            root_sig_ = root_sig;
        }

    private:
        ComPtr<ID3D12RootSignature> root_sig_;
    };

    class D3D12LocalRootSignatureSubobject final : public D3D12StateSubObject
    {
    public:
        explicit D3D12LocalRootSignatureSubobject(D3D12StateObjectDescHelper& parent)
        {
            subobject_ = &parent.CreateSubobject(D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, root_sig_.PutVoid());
        }

        void SetRootSignature(ID3D12RootSignature* root_sig) noexcept
        {
            root_sig_ = root_sig;
        }

    private:
        ComPtr<ID3D12RootSignature> root_sig_;
    };

    void PrintStateObjectDesc(D3D12_STATE_OBJECT_DESC const& desc)
    {
        std::wostringstream ss;
        ss << L"\n--------------------------------------------------------------------\n";
        ss << L"| D3D12 State Object 0x" << static_cast<void const*>(&desc) << L": ";
        if (desc.Type == D3D12_STATE_OBJECT_TYPE_COLLECTION)
        {
            ss << L"Collection\n";
        }
        else if (desc.Type == D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE)
        {
            ss << L"Raytracing Pipeline\n";
        }

        auto ExportTree = [](std::wostringstream& ss, uint32_t depth, uint32_t num_exports, D3D12_EXPORT_DESC const* exports) {
            for (uint32_t i = 0; i < num_exports; ++i)
            {
                ss << L"|";
                if (depth > 0)
                {
                    for (uint32_t j = 0; j < 2 * depth - 1; ++j)
                    {
                        ss << L" ";
                    }
                }
                ss << L" [" << i << L"]: ";
                if (exports[i].ExportToRename)
                {
                    ss << exports[i].ExportToRename << L" --> ";
                }
                ss << exports[i].Name << L"\n";
            }
        };

        for (uint32_t i = 0; i < desc.NumSubobjects; ++i)
        {
            ss << L"| [" << i << L"]: ";
            switch (desc.pSubobjects[i].Type)
            {
            case D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE:
                ss << L"Global Root Signature 0x" << desc.pSubobjects[i].pDesc << L"\n";
                break;
            case D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE:
                ss << L"Local Root Signature 0x" << desc.pSubobjects[i].pDesc << L"\n";
                break;
            case D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK:
                ss << L"Node Mask: 0x" << std::hex << std::setfill(L'0') << std::setw(8)
                   << *static_cast<uint32_t const*>(desc.pSubobjects[i].pDesc) << std::setw(0) << std::dec << L"\n";
                break;
            case D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY:
            {
                auto const& lib = *static_cast<D3D12_DXIL_LIBRARY_DESC const*>(desc.pSubobjects[i].pDesc);
                ss << L"DXIL Library 0x" << lib.DXILLibrary.pShaderBytecode << L", " << lib.DXILLibrary.BytecodeLength << L" bytes\n";
                ExportTree(ss, 1, lib.NumExports, lib.pExports);
                break;
            }
            case D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION:
            {
                auto const& collection = *static_cast<D3D12_EXISTING_COLLECTION_DESC const*>(desc.pSubobjects[i].pDesc);
                ss << L"Existing Library 0x" << collection.pExistingCollection << L"\n";
                ExportTree(ss, 1, collection.NumExports, collection.pExports);
                break;
            }
            case D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
            {
                auto const& association = *static_cast<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION const*>(desc.pSubobjects[i].pDesc);
                uint32_t const index = static_cast<uint32_t>(association.pSubobjectToAssociate - desc.pSubobjects);
                ss << L"Subobject to Exports Association (Subobject [" << index << L"])\n";
                for (uint32_t j = 0; j < association.NumExports; ++j)
                {
                    ss << L"|  [" << j << L"]: " << association.pExports[j] << L"\n";
                }
                break;
            }
            case D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
            {
                auto const& association = *static_cast<D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION const*>(desc.pSubobjects[i].pDesc);
                ss << L"DXIL Subobjects to Exports Association (" << association.SubobjectToAssociate << L")\n";
                for (uint32_t j = 0; j < association.NumExports; ++j)
                {
                    ss << L"|  [" << j << L"]: " << association.pExports[j] << L"\n";
                }
                break;
            }
            case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG:
            {
                auto const& config = *static_cast<D3D12_RAYTRACING_SHADER_CONFIG const*>(desc.pSubobjects[i].pDesc);
                ss << L"Raytracing Shader Config\n";
                ss << L"|  [0]: Max Payload Size: " << config.MaxPayloadSizeInBytes << L" bytes\n";
                ss << L"|  [1]: Max Attribute Size: " << config.MaxAttributeSizeInBytes << L" bytes\n";
                break;
            }
            case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG:
            {
                auto const& config = *static_cast<D3D12_RAYTRACING_PIPELINE_CONFIG const*>(desc.pSubobjects[i].pDesc);
                ss << L"Raytracing Pipeline Config\n";
                ss << L"|  [0]: Max Recursion Depth: " << config.MaxTraceRecursionDepth << L"\n";
                break;
            }
            case D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP:
            {
                auto const& hit_group = *static_cast<D3D12_HIT_GROUP_DESC const*>(desc.pSubobjects[i].pDesc);
                ss << L"Hit Group (" << (hit_group.HitGroupExport ? hit_group.HitGroupExport : L"[none]") << L")\n";
                ss << L"|  [0]: Any Hit Import: " << (hit_group.AnyHitShaderImport ? hit_group.AnyHitShaderImport : L"[none]") << L"\n";
                ss << L"|  [1]: Closest Hit Import: " << (hit_group.ClosestHitShaderImport ? hit_group.ClosestHitShaderImport : L"[none]")
                   << L"\n";
                ss << L"|  [2]: Intersection Import: "
                   << (hit_group.IntersectionShaderImport ? hit_group.IntersectionShaderImport : L"[none]") << L"\n";
                break;
            }
            }
            ss << L"|--------------------------------------------------------------------\n";
        }
        ss << L"\n";
        ::OutputDebugStringW(ss.str().c_str());
    }

    bool IsDXRSupported(ID3D12Device5* device)
    {
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 feature_support_data{};
        HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &feature_support_data, sizeof(feature_support_data));
        return SUCCEEDED(hr) && (feature_support_data.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
    }

    void UploadTexture(ID3D12Device5* device, ID3D12GraphicsCommandList4* cmd_list, ID3D12Resource* texture, void const* data)
    {
        auto const tex_desc = texture->GetDesc();
        uint32_t const width = static_cast<uint32_t>(tex_desc.Width);
        uint32_t const height = tex_desc.Height;

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint32_t num_row = 0;
        uint64_t row_size_in_bytes = 0;
        uint64_t required_size = 0;
        device->GetCopyableFootprints(&tex_desc, 0, 1, 0, &layout, &num_row, &row_size_in_bytes, &required_size);

        GpuUploadBuffer upload_buffer(device, required_size, L"UploadBuffer");

        assert(row_size_in_bytes >= width * 4);

        uint8_t* tex_data = upload_buffer.MappedData<uint8_t>();
        for (uint32_t y = 0; y < height; ++y)
        {
            memcpy(tex_data + y * row_size_in_bytes, static_cast<uint8_t const*>(data) + y * width * 4, width * 4);
        }

        D3D12_TEXTURE_COPY_LOCATION src;
        src.pResource = upload_buffer.Resource();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layout;

        D3D12_TEXTURE_COPY_LOCATION dst;
        dst.pResource = texture;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;

        D3D12_BOX src_box;
        src_box.left = 0;
        src_box.top = 0;
        src_box.front = 0;
        src_box.right = width;
        src_box.bottom = height;
        src_box.back = 1;

        D3D12_RESOURCE_BARRIER barrier;
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = texture;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd_list->ResourceBarrier(1, &barrier);

        cmd_list->CopyTextureRegion(&dst, 0, 0, 0, &src, &src_box);

        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        cmd_list->ResourceBarrier(1, &barrier);

        // TODO: LEAK!! Cache the resource until the command list is executed
        upload_buffer.Resource()->AddRef();
    }

    ComPtr<ID3D12Resource> CreateSolidColorTexture(
        ID3D12Device5* device, ID3D12GraphicsCommandList4* cmd_list, DirectX::PackedVector::XMCOLOR fill_color)
    {
        ComPtr<ID3D12Resource> ret;

        D3D12_HEAP_PROPERTIES const default_heap_prop = {
            D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};

        D3D12_RESOURCE_DESC const tex_desc = {D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, 1, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0},
            D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};
        TIFHR(device->CreateCommittedResource(&default_heap_prop, D3D12_HEAP_FLAG_NONE, &tex_desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr, UuidOf<ID3D12Resource>(), ret.PutVoid()));

        UploadTexture(device, cmd_list, ret.Get(), &fill_color);

        return ret;
    }

    struct SceneConstantBuffer
    {
        XMFLOAT4X4 inv_view_proj;
        XMFLOAT4 camera_pos;

        XMFLOAT4 light_pos;
        XMFLOAT4 light_color;
    };

    struct PrimitiveConstantBuffer
    {
        uint32_t material_id;
        uint32_t padding[3];
    };

    struct GlobalRootSignature
    {
        enum class Slot : uint32_t
        {
            OutputView = 0,
            AccelerationStructure,
            SceneConstant,
            MaterialBuffers,
            Count
        };
    };

    struct LocalRootSignature
    {
        enum class Slot : uint32_t
        {
            PrimitiveConstant = 0,
            VertexBuffer,
            IndexBuffer,
            AlbedoTex,
            MetalnessGlossinessTex,
            EmissiveTex,
            NormalTex,
            OcclusionTex,
            Count
        };

        struct RootArguments
        {
            PrimitiveConstantBuffer cb;

            D3D12_GPU_DESCRIPTOR_HANDLE vb_gpu_handle;
            D3D12_GPU_DESCRIPTOR_HANDLE ib_gpu_handle;

            D3D12_GPU_DESCRIPTOR_HANDLE albedo_gpu_handle;
            D3D12_GPU_DESCRIPTOR_HANDLE metalness_glossiness_gpu_handle;
            D3D12_GPU_DESCRIPTOR_HANDLE emissive_gpu_handle;
            D3D12_GPU_DESCRIPTOR_HANDLE normal_gpu_handle;
            D3D12_GPU_DESCRIPTOR_HANDLE occlusion_gpu_handle;
        };
    };

    struct Buffer
    {
        ComPtr<ID3D12Resource> resource;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_descriptor_handle;
    };
} // namespace

namespace GoldenSun
{
    class Engine::EngineImpl
    {
        DISALLOW_COPY_AND_ASSIGN(EngineImpl);
        DISALLOW_COPY_MOVE_AND_ASSIGN(EngineImpl);

    public:
        EngineImpl(ID3D12Device5* device, ID3D12CommandQueue* cmd_queue)
            : device_(device), cmd_queue_(cmd_queue), per_frame_constants_(device_.Get(), FrameCount, L"Per Frame Constants")
        {
            Verify(IsDXRSupported(device_.Get()));

            for (auto& allocator : cmd_allocators_)
            {
                TIFHR(
                    device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, UuidOf<ID3D12CommandAllocator>(), allocator.PutVoid()));
            }

            TIFHR(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_allocators_[0].Get(), nullptr,
                UuidOf<ID3D12GraphicsCommandList4>(), cmd_list_.PutVoid()));
            TIFHR(cmd_list_->Close());

            uint32_t constexpr MaxNumBottomLevelInstances = 1000;
            acceleration_structure_ =
                std::make_unique<RaytracingAccelerationStructureManager>(device_.Get(), MaxNumBottomLevelInstances, FrameCount);

            this->CreateRootSignatures();
            this->CreateRayTracingPipelineStateObject();
            this->CreateDescriptorHeap();
        }

        ~EngineImpl() noexcept
        {
            this->ReleaseWindowSizeDependentResources();
        }

        void RenderTarget(uint32_t width, uint32_t height, DXGI_FORMAT format)
        {
            if ((width != width_) || (height != height_) || (format != format_))
            {
                width_ = width;
                height_ = height;
                format_ = format;

                this->ReleaseWindowSizeDependentResources();

                if ((width_ > 0) && (height_ > 0))
                {
                    aspect_ratio_ = static_cast<float>(width) / height;
                    this->CreateWindowSizeDependentResources();
                }
            }
        }

        void Geometries(ID3D12GraphicsCommandList4* cmd_list, Mesh const* meshes, uint32_t num_meshes)
        {
            {
                material_start_indices_.assign(1, 0);
                for (uint32_t i = 0; i < num_meshes; ++i)
                {
                    material_start_indices_.push_back(material_start_indices_.back() + meshes[i].NumMaterials());
                }

                uint32_t const num_materials = material_start_indices_.back();
                StructuredBuffer<PbrMaterial::Buffer> mb(device_.Get(), num_materials, 1, L"Material Buffer");
                for (uint32_t i = 0; i < num_meshes; ++i)
                {
                    for (uint32_t j = 0; j < meshes[i].NumMaterials(); ++j)
                    {
                        mb[material_start_indices_[i] + j] = meshes[i].Material(j).buffer;
                    }
                }
                mb.UploadToGpu();

                material_buffer_.resource = mb.Resource();
                this->CreateBufferSrv(material_buffer_, num_materials, sizeof(PbrMaterial::Buffer));
            }

            {
                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags =
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
                for (uint32_t i = 0; i < num_meshes; ++i)
                {
                    auto& vb = vertex_buffers_.emplace_back();
                    vb.resource = meshes[i].VertexBuffer(0);
                    uint32_t const descriptor_index_vb =
                        this->CreateBufferSrv(vb, meshes[i].NumVertices(0), meshes[i].VertexStrideInBytes());

                    auto& ib = index_buffers_.emplace_back();
                    ib.resource = meshes[i].IndexBuffer(0);
                    uint32_t const descriptor_index_ib =
                        this->CreateBufferSrv(ib, meshes[i].NumIndices(0) * meshes[i].IndexStrideInBytes() / 4, 0);

                    Verify(descriptor_index_ib == descriptor_index_vb + 1);

                    bool update_on_build = false;
                    acceleration_structure_->AddBottomLevelAS(device_.Get(), build_flags, meshes[i], update_on_build, update_on_build);
                }
            }

            this->BuildShaderTables(cmd_list, meshes, num_meshes);

            for (uint32_t i = 0; i < num_meshes; ++i)
            {
                for (uint32_t j = 0; j < meshes[i].NumInstances(); ++j)
                {
                    acceleration_structure_->AddBottomLevelASInstance(i, 0xFFFFFFFFU, XMLoadFloat4x4(&meshes[i].Transform(j)));
                }
            }

            {
                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags =
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
                bool allow_update = false;
                bool perform_update_on_build = false;
                acceleration_structure_->ResetTopLevelAS(
                    device_.Get(), build_flags, allow_update, perform_update_on_build, L"Top-Level Acceleration Structure");
            }
        }

        void Camera(XMFLOAT3 const& eye, XMFLOAT3 const& look_at, XMFLOAT3 const& up, float fov, float near_plane, float far_plane)
        {
            eye_ = eye;
            look_at_ = look_at;
            up_ = up;

            fov_ = fov;
            near_plane_ = near_plane;
            far_plane_ = far_plane;
        }

        void Light(XMFLOAT3 const& pos, XMFLOAT3 const& color)
        {
            light_pos_ = pos;
            light_color_ = color;
        }

        void Render(ID3D12GraphicsCommandList4* cmd_list)
        {
            acceleration_structure_->Build(cmd_list, frame_index_);

            cmd_list->SetComputeRootSignature(ray_tracing_global_root_signature_.Get());

            {
                per_frame_constants_->camera_pos = XMFLOAT4(eye_.x, eye_.y, eye_.z, 1);

                auto const view = XMMatrixLookAtLH(XMLoadFloat3(&eye_), XMLoadFloat3(&look_at_), XMLoadFloat3(&up_));
                auto const proj = XMMatrixPerspectiveFovLH(fov_, aspect_ratio_, near_plane_, far_plane_);
                XMStoreFloat4x4(&per_frame_constants_->inv_view_proj, XMMatrixTranspose(XMMatrixInverse(nullptr, view * proj)));

                per_frame_constants_->light_pos = XMFLOAT4(light_pos_.x, light_pos_.y, light_pos_.z, 1);
                per_frame_constants_->light_color = XMFLOAT4(light_color_.x, light_color_.y, light_color_.z, 1);

                per_frame_constants_.UploadToGpu(frame_index_);

                cmd_list->SetComputeRootConstantBufferView(
                    ConvertToUint(GlobalRootSignature::Slot::SceneConstant), per_frame_constants_.GpuVirtualAddress(frame_index_));
            }

            ID3D12DescriptorHeap* heaps[] = {descriptor_heap_.Get()};
            cmd_list->SetDescriptorHeaps(static_cast<uint32_t>(std::size(heaps)), heaps);
            cmd_list->SetComputeRootDescriptorTable(
                ConvertToUint(GlobalRootSignature::Slot::OutputView), ray_tracing_output_resource_uav_gpu_descriptor_handle_);
            cmd_list->SetComputeRootShaderResourceView(ConvertToUint(GlobalRootSignature::Slot::AccelerationStructure),
                acceleration_structure_->TopLevelASResource()->GetGPUVirtualAddress());
            cmd_list->SetComputeRootDescriptorTable(
                ConvertToUint(GlobalRootSignature::Slot::MaterialBuffers), material_buffer_.gpu_descriptor_handle);

            D3D12_DISPATCH_RAYS_DESC dispatch_desc{};

            dispatch_desc.HitGroupTable.StartAddress = hit_group_shader_table_->GetGPUVirtualAddress();
            dispatch_desc.HitGroupTable.SizeInBytes = hit_group_shader_table_->GetDesc().Width;
            dispatch_desc.HitGroupTable.StrideInBytes = hit_group_shader_table_stride_;

            dispatch_desc.MissShaderTable.StartAddress = miss_shader_table_->GetGPUVirtualAddress();
            dispatch_desc.MissShaderTable.SizeInBytes = miss_shader_table_->GetDesc().Width;
            dispatch_desc.MissShaderTable.StrideInBytes = miss_shader_table_stride_;

            dispatch_desc.RayGenerationShaderRecord.StartAddress = ray_gen_shader_table_->GetGPUVirtualAddress();
            dispatch_desc.RayGenerationShaderRecord.SizeInBytes = ray_gen_shader_table_->GetDesc().Width;

            dispatch_desc.Width = width_;
            dispatch_desc.Height = height_;
            dispatch_desc.Depth = 1;

            cmd_list->SetPipelineState1(state_obj_.Get());
            cmd_list->DispatchRays(&dispatch_desc);

            frame_index_ = (frame_index_ + 1) % FrameCount;
        }

        ID3D12Resource* Output() const noexcept
        {
            return ray_tracing_output_.Get();
        }

    private:
        void CreateWindowSizeDependentResources()
        {
            D3D12_HEAP_PROPERTIES const default_heap_prop = {
                D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};

            D3D12_RESOURCE_DESC const tex_desc = {D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, width_, height_, 1, 1, format_, {1, 0},
                D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};
            TIFHR(device_->CreateCommittedResource(&default_heap_prop, D3D12_HEAP_FLAG_NONE, &tex_desc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, UuidOf<ID3D12Resource>(), ray_tracing_output_.PutVoid()));
            ray_tracing_output_->SetName(L"GoldenSun Output");

            D3D12_CPU_DESCRIPTOR_HANDLE uav_desc_handle;
            ray_tracing_output_resource_uav_descriptor_heap_index_ =
                this->AllocateDescriptor(uav_desc_handle, ray_tracing_output_resource_uav_descriptor_heap_index_);
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
            uav_desc.Format = LinearFormatOf(format_);
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            device_->CreateUnorderedAccessView(ray_tracing_output_.Get(), nullptr, &uav_desc, uav_desc_handle);
            ray_tracing_output_resource_uav_gpu_descriptor_handle_ = {
                descriptor_heap_->GetGPUDescriptorHandleForHeapStart().ptr +
                ray_tracing_output_resource_uav_descriptor_heap_index_ * descriptor_size_};
        }

        void ReleaseWindowSizeDependentResources() noexcept
        {
            ray_tracing_output_.Reset();
        }

        ComPtr<ID3D12RootSignature> SerializeAndCreateRayTracingRootSignature(D3D12_ROOT_SIGNATURE_DESC const& desc)
        {
            ComPtr<ID3DBlob> blob;
            ComPtr<ID3DBlob> error;
            HRESULT hr = ::D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, blob.Put(), error.Put());
            if (FAILED(hr))
            {
                ::OutputDebugStringW(
                    (std::wstring(L"D3D12SerializeRootSignature failed: ") + static_cast<wchar_t*>(error->GetBufferPointer()) + L"\n")
                        .c_str());
                TIFHR(hr);
            }

            ComPtr<ID3D12RootSignature> root_sig;
            TIFHR(device_->CreateRootSignature(
                1, blob->GetBufferPointer(), blob->GetBufferSize(), UuidOf<ID3D12RootSignature>(), root_sig.PutVoid()));
            return root_sig;
        }

        void CreateRootSignatures()
        {
            {
                D3D12_DESCRIPTOR_RANGE const ranges[] = {
                    {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}, // Output
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}, // Material
                };

                D3D12_ROOT_PARAMETER const root_params[] = {
                    CreateRootParameterAsDescriptorTable(&ranges[0], 1),
                    CreateRootParameterAsShaderResourceView(0), // AccelerationStructure
                    CreateRootParameterAsConstantBufferView(0), // scene_cb
                    CreateRootParameterAsDescriptorTable(&ranges[1], 1),
                };

                D3D12_STATIC_SAMPLER_DESC sampler{};
                sampler.Filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
                sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                sampler.MaxLOD = D3D12_FLOAT32_MAX;
                sampler.ShaderRegister = 0;
                sampler.RegisterSpace = 0;
                sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

                D3D12_ROOT_SIGNATURE_DESC const global_root_signature_desc = {
                    static_cast<uint32_t>(std::size(root_params)), root_params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE};

                ray_tracing_global_root_signature_ = this->SerializeAndCreateRayTracingRootSignature(global_root_signature_desc);
            }

            {
                D3D12_DESCRIPTOR_RANGE const ranges[] = {
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}, // VB
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}, // IB
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}, // albedo
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}, // metalness glossiness
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}, // emissive
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}, // normal
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}, // occlusion
                };

                D3D12_ROOT_PARAMETER const root_params[] = {
                    CreateRootParameterAsConstants(Align<sizeof(uint32_t)>(sizeof(PrimitiveConstantBuffer)) / sizeof(uint32_t), 0, 1),
                    CreateRootParameterAsDescriptorTable(&ranges[0], 1),
                    CreateRootParameterAsDescriptorTable(&ranges[1], 1),
                    CreateRootParameterAsDescriptorTable(&ranges[2], 1),
                    CreateRootParameterAsDescriptorTable(&ranges[3], 1),
                    CreateRootParameterAsDescriptorTable(&ranges[4], 1),
                    CreateRootParameterAsDescriptorTable(&ranges[5], 1),
                    CreateRootParameterAsDescriptorTable(&ranges[6], 1),
                };

                D3D12_ROOT_SIGNATURE_DESC const local_root_signature_desc = {
                    static_cast<uint32_t>(std::size(root_params)), root_params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE};

                ray_tracing_local_root_signature_ = this->SerializeAndCreateRayTracingRootSignature(local_root_signature_desc);
            }
        }

        void CreateRayTracingPipelineStateObject()
        {
            D3D12StateObjectDescHelper ray_tracing_pipeline(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

            auto& lib = ray_tracing_pipeline.CreateSubobject<D3D12DxilLibrarySubObject>();
            D3D12_SHADER_BYTECODE const lib_dxil = {static_cast<void const*>(RayTracing_shader), std::size(RayTracing_shader)};
            lib.SetDxilLibrary(&lib_dxil);
            lib.DefineExport(c_ray_gen_shader_name);
            lib.DefineExport(c_closest_hit_shader_name);
            lib.DefineExport(c_miss_shader_name);

            auto& hit_group = ray_tracing_pipeline.CreateSubobject<D3D12HitGroupSubobject>();
            hit_group.SetClosestHitShaderImport(c_closest_hit_shader_name);
            hit_group.SetHitGroupExport(c_hit_group_name);
            hit_group.SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

            auto& shader_config = ray_tracing_pipeline.CreateSubobject<D3D12RayTracingShaderConfigSubobject>();
            shader_config.Config(sizeof(RayPayload), sizeof(RayAttrib));

            auto& local_root_signature = ray_tracing_pipeline.CreateSubobject<D3D12LocalRootSignatureSubobject>();
            local_root_signature.SetRootSignature(ray_tracing_local_root_signature_.Get());
            {
                auto& root_signature_association = ray_tracing_pipeline.CreateSubobject<D3D12SubobjectToExportsAssociationSubobject>();
                root_signature_association.SetSubobjectToAssociate(local_root_signature.Get());
                root_signature_association.AddExport(c_hit_group_name);
            }

            auto& global_root_signature = ray_tracing_pipeline.CreateSubobject<D3D12GlobalRootSignatureSubobject>();
            global_root_signature.SetRootSignature(ray_tracing_global_root_signature_.Get());

            auto& pipeline_config = ray_tracing_pipeline.CreateSubobject<D3D12RayTracingPipelineConfigSubobject>();
            pipeline_config.Config(1);

            auto const& state_obj_desc = ray_tracing_pipeline.Retrieve();

#ifdef _DEBUG
            PrintStateObjectDesc(state_obj_desc);
#endif

            TIFHR(device_->CreateStateObject(&state_obj_desc, UuidOf<ID3D12StateObject>(), state_obj_.PutVoid()));
        }

        void CreateDescriptorHeap()
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc_heap_desc{};
            desc_heap_desc.NumDescriptors = 256;
            desc_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            desc_heap_desc.NodeMask = 0;
            TIFHR(device_->CreateDescriptorHeap(&desc_heap_desc, UuidOf<ID3D12DescriptorHeap>(), descriptor_heap_.PutVoid()));
            descriptor_heap_->SetName(L"GoldenSun CBV SRV UAV Descriptor Heap");

            descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        void BuildShaderTables(ID3D12GraphicsCommandList4* cmd_list, Mesh const* meshes, uint32_t num_meshes)
        {
            void* ray_gen_shader_identifier;
            void* hit_group_shader_identifier;
            void* miss_shader_identifier;
            uint32_t shader_identifier_size;
            {
                ComPtr<ID3D12StateObjectProperties> state_obj_properties = state_obj_.As<ID3D12StateObjectProperties>();
                ray_gen_shader_identifier = state_obj_properties->GetShaderIdentifier(c_ray_gen_shader_name);
                miss_shader_identifier = state_obj_properties->GetShaderIdentifier(c_miss_shader_name);
                hit_group_shader_identifier = state_obj_properties->GetShaderIdentifier(c_hit_group_name);
                shader_identifier_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
            }

            {
                uint32_t const num_shader_records = 1;
                uint32_t const shader_record_size = shader_identifier_size;
                ShaderTable ray_gen_shader_table(device_.Get(), num_shader_records, shader_record_size, L"RayGenShaderTable");
                ray_gen_shader_table.push_back(ShaderRecord(ray_gen_shader_identifier, shader_identifier_size));

                ray_gen_shader_table_ = ray_gen_shader_table.Resource();
            }
            {
                uint32_t const num_shader_records = 1;
                uint32_t const shader_record_size = shader_identifier_size;
                ShaderTable miss_shader_table(device_.Get(), num_shader_records, shader_record_size, L"MissShaderTable");
                miss_shader_table.push_back(ShaderRecord(miss_shader_identifier, shader_identifier_size));

                miss_shader_table_stride_ = miss_shader_table.ShaderRecordSize();
                miss_shader_table_ = miss_shader_table.Resource();
            }

            {
                uint32_t num_shader_records = 0;
                for (uint32_t i = 0; i < num_meshes; ++i)
                {
                    num_shader_records += meshes[i].NumPrimitives();
                }

                uint32_t const shader_record_size = shader_identifier_size + sizeof(LocalRootSignature::RootArguments);
                ShaderTable hit_group_shader_table(device_.Get(), num_shader_records, shader_record_size, L"HitGroupShaderTable");
                for (uint32_t i = 0; i < num_meshes; ++i)
                {
                    auto& geometry = acceleration_structure_->BottomLevelAS(i);
                    uint32_t const shader_record_offset = hit_group_shader_table.NumShaderRecords();
                    geometry.InstanceContributionToHitGroupIndex(shader_record_offset);

                    for (uint32_t j = 0; j < meshes[i].NumPrimitives(); ++j)
                    {
                        LocalRootSignature::RootArguments root_arguments;
                        root_arguments.cb.material_id = material_start_indices_[i] + meshes[i].MaterialId(j);
                        root_arguments.vb_gpu_handle = vertex_buffers_[i].gpu_descriptor_handle;
                        root_arguments.ib_gpu_handle = index_buffers_[i].gpu_descriptor_handle;

                        auto const& material = meshes[i].Material(meshes[i].MaterialId(j));

                        auto* albedo_tex = material.textures[ConvertToUint(PbrMaterial::TextureSlot::Albedo)].Get();
                        if (albedo_tex == nullptr)
                        {
                            if (!default_textures_[ConvertToUint(PbrMaterial::TextureSlot::Albedo)])
                            {
                                default_textures_[ConvertToUint(PbrMaterial::TextureSlot::Albedo)] =
                                    CreateSolidColorTexture(device_.Get(), cmd_list, PackedVector::XMCOLOR(1, 1, 1, 1));
                            }

                            albedo_tex = default_textures_[ConvertToUint(PbrMaterial::TextureSlot::Albedo)].Get();
                        }
                        auto* metalness_glossiness_tex =
                            material.textures[ConvertToUint(PbrMaterial::TextureSlot::MetalnessGlossiness)].Get();
                        if (metalness_glossiness_tex == nullptr)
                        {
                            if (!default_textures_[ConvertToUint(PbrMaterial::TextureSlot::MetalnessGlossiness)])
                            {
                                default_textures_[ConvertToUint(PbrMaterial::TextureSlot::MetalnessGlossiness)] =
                                    CreateSolidColorTexture(device_.Get(), cmd_list, PackedVector::XMCOLOR(0, 1, 0, 0));
                            }

                            metalness_glossiness_tex =
                                default_textures_[ConvertToUint(PbrMaterial::TextureSlot::MetalnessGlossiness)].Get();
                        }
                        auto* emissive_tex = material.textures[ConvertToUint(PbrMaterial::TextureSlot::Emissive)].Get();
                        if (emissive_tex == nullptr)
                        {
                            if (!default_textures_[ConvertToUint(PbrMaterial::TextureSlot::Emissive)])
                            {
                                default_textures_[ConvertToUint(PbrMaterial::TextureSlot::Emissive)] =
                                    CreateSolidColorTexture(device_.Get(), cmd_list, PackedVector::XMCOLOR(0, 0, 0, 0));
                            }

                            emissive_tex = default_textures_[ConvertToUint(PbrMaterial::TextureSlot::Emissive)].Get();
                        }
                        auto* normal_tex = material.textures[ConvertToUint(PbrMaterial::TextureSlot::Normal)].Get();
                        if (normal_tex == nullptr)
                        {
                            if (!default_textures_[ConvertToUint(PbrMaterial::TextureSlot::Normal)])
                            {
                                default_textures_[ConvertToUint(PbrMaterial::TextureSlot::Normal)] =
                                    CreateSolidColorTexture(device_.Get(), cmd_list, PackedVector::XMCOLOR(0, 0, 1, 0));
                            }

                            normal_tex = default_textures_[ConvertToUint(PbrMaterial::TextureSlot::Normal)].Get();
                        }
                        auto* occlusion_tex = material.textures[ConvertToUint(PbrMaterial::TextureSlot::Occlusion)].Get();
                        if (occlusion_tex == nullptr)
                        {
                            if (!default_textures_[ConvertToUint(PbrMaterial::TextureSlot::Occlusion)])
                            {
                                default_textures_[ConvertToUint(PbrMaterial::TextureSlot::Occlusion)] =
                                    CreateSolidColorTexture(device_.Get(), cmd_list, PackedVector::XMCOLOR(1, 1, 1, 1));
                            }

                            occlusion_tex = default_textures_[ConvertToUint(PbrMaterial::TextureSlot::Occlusion)].Get();
                        }

                        root_arguments.albedo_gpu_handle = this->CreateTextureSrv(albedo_tex);
                        root_arguments.metalness_glossiness_gpu_handle = this->CreateTextureSrv(metalness_glossiness_tex);
                        root_arguments.emissive_gpu_handle = this->CreateTextureSrv(emissive_tex);
                        root_arguments.normal_gpu_handle = this->CreateTextureSrv(normal_tex);
                        root_arguments.occlusion_gpu_handle = this->CreateTextureSrv(occlusion_tex);

                        hit_group_shader_table.push_back(
                            ShaderRecord(hit_group_shader_identifier, shader_identifier_size, &root_arguments, sizeof(root_arguments)));
                    }
                }

                hit_group_shader_table_stride_ = hit_group_shader_table.ShaderRecordSize();
                hit_group_shader_table_ = hit_group_shader_table.Resource();
            }
        }

        uint32_t AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& cpu_descriptor, uint32_t descriptor_index_to_use = 0xFFFFFFFFU)
        {
            auto const descriptor_heap_cpu_base = descriptor_heap_->GetCPUDescriptorHandleForHeapStart();
            if (descriptor_index_to_use >= descriptor_heap_->GetDesc().NumDescriptors)
            {
                descriptor_index_to_use = descriptors_allocated_;
                ++descriptors_allocated_;
            }
            cpu_descriptor = {descriptor_heap_cpu_base.ptr + descriptor_index_to_use * descriptor_size_};
            return descriptor_index_to_use;
        }

        uint32_t CreateBufferSrv(Buffer& buffer, uint32_t num_elements, uint32_t element_size)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Buffer.NumElements = num_elements;
            srv_desc.Buffer.StructureByteStride = element_size;
            if (element_size == 0)
            {
                srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
                srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            }
            else
            {
                srv_desc.Format = DXGI_FORMAT_UNKNOWN;
                srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            }
            uint32_t const descriptor_index = this->AllocateDescriptor(buffer.cpu_descriptor_handle);
            device_->CreateShaderResourceView(buffer.resource.Get(), &srv_desc, buffer.cpu_descriptor_handle);
            buffer.gpu_descriptor_handle = {
                descriptor_heap_->GetGPUDescriptorHandleForHeapStart().ptr + descriptor_index * descriptor_size_};
            return descriptor_index;
        }

        D3D12_GPU_DESCRIPTOR_HANDLE CreateTextureSrv(ID3D12Resource* texture)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.Format = texture->GetDesc().Format;
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Texture2D.MostDetailedMip = 0;
            srv_desc.Texture2D.MipLevels = texture->GetDesc().MipLevels;
            srv_desc.Texture2D.PlaneSlice = 0;
            srv_desc.Texture2D.ResourceMinLODClamp = 0;

            D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle;
            uint32_t const descriptor_index = this->AllocateDescriptor(cpu_desc_handle);
            device_->CreateShaderResourceView(texture, &srv_desc, cpu_desc_handle);
            D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle = {
                descriptor_heap_->GetGPUDescriptorHandleForHeapStart().ptr + descriptor_index * descriptor_size_};
            return gpu_desc_handle;
        }

    private:
        ComPtr<ID3D12Device5> device_;
        ComPtr<ID3D12CommandQueue> cmd_queue_;
        ComPtr<ID3D12GraphicsCommandList4> cmd_list_;
        ComPtr<ID3D12CommandAllocator> cmd_allocators_[FrameCount];
        ComPtr<ID3D12StateObject> state_obj_;

        uint32_t frame_index_ = 0;

        uint32_t width_ = 0;
        uint32_t height_ = 0;
        float aspect_ratio_ = 0;
        DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;

        ConstantBuffer<SceneConstantBuffer> per_frame_constants_;

        ComPtr<ID3D12RootSignature> ray_tracing_global_root_signature_;
        ComPtr<ID3D12RootSignature> ray_tracing_local_root_signature_;

        ComPtr<ID3D12DescriptorHeap> descriptor_heap_;
        uint32_t descriptors_allocated_ = 0;
        uint32_t descriptor_size_;

        Buffer material_buffer_;
        std::vector<uint32_t> material_start_indices_;
        std::array<ComPtr<ID3D12Resource>, ConvertToUint(PbrMaterial::TextureSlot::Num)> default_textures_;

        std::vector<Buffer> vertex_buffers_;
        std::vector<Buffer> index_buffers_;

        std::unique_ptr<RaytracingAccelerationStructureManager> acceleration_structure_;

        ComPtr<ID3D12Resource> ray_tracing_output_;
        D3D12_GPU_DESCRIPTOR_HANDLE ray_tracing_output_resource_uav_gpu_descriptor_handle_;
        uint32_t ray_tracing_output_resource_uav_descriptor_heap_index_ = 0xFFFFFFFFU;

        static constexpr wchar_t c_ray_gen_shader_name[] = L"RayGenShader";
        static constexpr wchar_t c_hit_group_name[] = L"HitGroup";
        static constexpr wchar_t c_closest_hit_shader_name[] = L"ClosestHitShader";
        static constexpr wchar_t c_miss_shader_name[] = L"MissShader";
        ComPtr<ID3D12Resource> ray_gen_shader_table_;
        ComPtr<ID3D12Resource> hit_group_shader_table_;
        uint32_t hit_group_shader_table_stride_ = 0xFFFFFFFFU;
        ComPtr<ID3D12Resource> miss_shader_table_;
        uint32_t miss_shader_table_stride_ = 0xFFFFFFFFU;

        XMFLOAT3 eye_;
        XMFLOAT3 look_at_;
        XMFLOAT3 up_;

        float fov_;
        float near_plane_;
        float far_plane_;

        XMFLOAT3 light_pos_;
        XMFLOAT3 light_color_;
    };


    Engine::Engine(ID3D12Device5* device, ID3D12CommandQueue* cmd_queue) : impl_(new EngineImpl(device, cmd_queue))
    {
    }

    Engine::~Engine()
    {
        delete impl_;
        impl_ = nullptr;
    }

    Engine::Engine(Engine&& other) noexcept : impl_(std::move(other.impl_))
    {
        other.impl_ = nullptr;
    }

    Engine& Engine::operator=(Engine&& other) noexcept
    {
        if (this != &other)
        {
            impl_ = std::move(other.impl_);
            other.impl_ = nullptr;
        }
        return *this;
    }

    void Engine::RenderTarget(uint32_t width, uint32_t height, DXGI_FORMAT format)
    {
        return impl_->RenderTarget(width, height, format);
    }

    void Engine::Geometries(ID3D12GraphicsCommandList4* cmd_list, Mesh const* meshes, uint32_t num_meshes)
    {
        return impl_->Geometries(cmd_list, meshes, num_meshes);
    }

    void Engine::Camera(XMFLOAT3 const& eye, XMFLOAT3 const& look_at, XMFLOAT3 const& up, float fov, float near_plane, float far_plane)
    {
        return impl_->Camera(eye, look_at, up, fov, near_plane, far_plane);
    }

    void Engine::Light(XMFLOAT3 const& pos, XMFLOAT3 const& color)
    {
        return impl_->Light(pos, color);
    }

    void Engine::Render(ID3D12GraphicsCommandList4* cmd_list)
    {
        return impl_->Render(cmd_list);
    }

    ID3D12Resource* Engine::Output() const noexcept
    {
        return impl_->Output();
    }
} // namespace GoldenSun
