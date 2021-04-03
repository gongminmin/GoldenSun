#include "pch.hpp"

#include <GoldenSun/Engine.hpp>

#include <GoldenSun/Camera.hpp>
#include <GoldenSun/Material.hpp>
#include <GoldenSun/Mesh.hpp>
#include <GoldenSun/SmartPtrHelper.hpp>
#include <GoldenSun/Util.hpp>

#include <cassert>
#include <iomanip>
#include <list>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <DirectXMath.h>

#include "AccelerationStructure.hpp"
#include "EngineInternal.hpp"

#include "CompiledShaders/RayTracing.h"

using namespace DirectX;
using namespace GoldenSun;

namespace
{
    static uint32_t constexpr MaxRayRecursionDepth = 3;

    struct RadianceRayPayload
    {
        XMFLOAT4 color;
        uint32_t recursion_depth;
    };
    static_assert(sizeof(RadianceRayPayload) == sizeof(XMFLOAT4) + sizeof(uint32_t));

    struct ShadowRayPayload
    {
        bool hit;
    };
    static_assert(sizeof(ShadowRayPayload) == sizeof(bool));

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
        ShaderTable(GpuSystem& gpu_system, uint32_t num_shader_records, uint32_t shader_record_size)
            : ShaderTable(gpu_system, num_shader_records, shader_record_size, L"")
        {
        }

        ShaderTable(GpuSystem& gpu_system, uint32_t max_num_shader_records, uint32_t shader_record_size, std::wstring_view resource_name)
            : shader_record_size_(Align<D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT>(shader_record_size)),
              buffer_(gpu_system.CreateUploadBuffer(max_num_shader_records * shader_record_size_, std::move(resource_name)))
        {
            mapped_shader_records_ = buffer_.MappedData<uint8_t>();
        }

        void ExtractBuffer(GpuUploadBuffer& buffer) noexcept
        {
            buffer = std::move(buffer_);
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
        D3D12_STATE_SUBOBJECT const* subobject_{};
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

        template <size_t N>
        void AddExports(wchar_t const* const (&exports)[N])
        {
            for (size_t i = 0; i < N; i++)
            {
                this->AddExport(exports[i]);
            }
        }

        void AddExports(wchar_t const* const* exports, uint32_t n)
        {
            for (uint32_t i = 0; i < n; i++)
            {
                this->AddExport(exports[i]);
            }
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

    GpuTexture2D CreateSolidColorTexture(GpuSystem& gpu_system, GpuCommandList& cmd_list, uint32_t fill_color_rgba)
    {
        auto texture = gpu_system.CreateTexture2D(
            1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
        texture.Upload(gpu_system, cmd_list, 0, &fill_color_rgba);
        return texture;
    }

    struct SceneConstantBuffer
    {
        XMFLOAT4X4 inv_view_proj;
        XMFLOAT4 camera_pos;
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
            MaterialBuffer,
            LightBuffer,
            Num
        };
    };

    struct LocalRootSignature
    {
        enum class Slot : uint32_t
        {
            PrimitiveConstant = 0,
            VertexBuffer,
            AlbedoTex,
            Num
        };

        struct RootArguments
        {
            PrimitiveConstantBuffer cb;

            D3D12_GPU_DESCRIPTOR_HANDLE buffer_gpu_handle;
            D3D12_GPU_DESCRIPTOR_HANDLE texture_gpu_handle;
        };
    };

    uint32_t constexpr MaxNumBottomLevelInstances = 1000;
} // namespace

namespace GoldenSun
{
    class Engine::Impl
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl(ID3D12Device5* device, ID3D12CommandQueue* cmd_queue)
            : gpu_system_(device, cmd_queue), per_frame_constants_(gpu_system_, GpuSystem::FrameCount(), L"Per Frame Constants"),
              descriptor_size_(gpu_system_.CbvSrvUavDescSize()), acceleration_structure_(gpu_system_, MaxNumBottomLevelInstances)
        {
            Verify(IsDXRSupported(device));

            this->CreateRootSignatures();
            this->CreateRayTracingPipelineStateObject();

            auto cmd_list = gpu_system_.CreateCommandList();
            default_textures_[std::to_underlying(PbrMaterial::TextureSlot::Albedo)] =
                CreateSolidColorTexture(gpu_system_, cmd_list, 0xFFFFFFFFU);
            default_textures_[std::to_underlying(PbrMaterial::TextureSlot::MetallicGlossiness)] =
                CreateSolidColorTexture(gpu_system_, cmd_list, 0x00000000U);
            default_textures_[std::to_underlying(PbrMaterial::TextureSlot::Emissive)] =
                CreateSolidColorTexture(gpu_system_, cmd_list, 0x00000000U);
            default_textures_[std::to_underlying(PbrMaterial::TextureSlot::Normal)] =
                CreateSolidColorTexture(gpu_system_, cmd_list, 0x00FF8080U);
            default_textures_[std::to_underlying(PbrMaterial::TextureSlot::Occlusion)] =
                CreateSolidColorTexture(gpu_system_, cmd_list, 0xFFFFFFFFU);
            gpu_system_.Execute(std::move(cmd_list));
        }

        ~Impl() noexcept
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

        void Meshes(Mesh const* meshes, uint32_t num_meshes)
        {
            primitive_start_.assign(1, 0);
            material_start_.assign(1, 0);
            for (uint32_t i = 0; i < num_meshes; ++i)
            {
                auto const& mesh = meshes[i];
                primitive_start_.emplace_back(primitive_start_.back() + mesh.NumPrimitives());
                material_start_.emplace_back(material_start_.back() + mesh.NumMaterials());
            }

            {
                uint32_t const num_primitives = primitive_start_.back();
                uint32_t const num_materials = material_start_.back();

                mesh_buffers_.clear();
                mesh_buffers_.reserve(num_primitives * 2);

                material_ids_.clear();
                material_ids_.reserve(num_primitives);
                uint32_t material_id = 0;
                uint32_t shader_record_offset = 0;

                uint32_t constexpr num_textures = std::to_underlying(PbrMaterial::TextureSlot::Num);
                material_texs_.clear();
                material_texs_.reserve(num_materials * num_textures);

                gpu_system_.ReallocUploadMemBlock(material_mem_block_, num_materials * sizeof(PbrMaterialBuffer));
                auto* material_mem = material_mem_block_.CpuAddress<PbrMaterialBuffer>();

                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags =
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
                for (uint32_t i = 0; i < num_meshes; ++i)
                {
                    auto const& mesh = meshes[i];

                    for (uint32_t j = 0; j < mesh.NumPrimitives(); ++j)
                    {
                        mesh_buffers_.emplace_back(MeshBuffer{GpuBuffer(mesh.VertexBuffer(j), D3D12_RESOURCE_STATE_GENERIC_READ),
                            mesh.NumVertices(j), mesh.VertexStrideInBytes()});
                        mesh_buffers_.emplace_back(MeshBuffer{GpuBuffer(mesh.IndexBuffer(j), D3D12_RESOURCE_STATE_GENERIC_READ),
                            mesh.NumIndices(j), mesh.IndexStrideInBytes()});

                        material_ids_.emplace_back(material_id + mesh.MaterialId(j));
                    }

                    bool update_on_build = false;
                    acceleration_structure_.AddBottomLevelAS(
                        gpu_system_, build_flags, mesh, shader_record_offset, update_on_build, update_on_build);

                    shader_record_offset += mesh.NumPrimitives() * std::to_underlying(RayType::Num);

                    for (uint32_t j = 0; j < mesh.NumMaterials(); ++j)
                    {
                        auto const& material = mesh.Material(j);
                        material_mem[material_id] = EngineInternal::Buffer(material);

                        for (uint32_t k = 0; k < num_textures; ++k)
                        {
                            auto& texture = material_texs_.emplace_back();
                            if (auto* slot = material.Texture(static_cast<PbrMaterial::TextureSlot>(k)))
                            {
                                texture = GpuTexture2D(slot, D3D12_RESOURCE_STATE_GENERIC_READ);
                            }
                            else
                            {
                                texture = default_textures_[k].Share();
                            }
                        }

                        ++material_id;
                    }

                    for (uint32_t j = 0; j < mesh.NumInstances(); ++j)
                    {
                        acceleration_structure_.AddBottomLevelASInstance(i, XMLoadFloat4x4(&mesh.Instance(j).transform));
                    }
                }
            }

            {
                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags =
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
                bool allow_update = false;
                bool perform_update_on_build = false;
                acceleration_structure_.AssignTopLevelAS(
                    gpu_system_, build_flags, allow_update, perform_update_on_build, L"Top-Level Acceleration Structure");
            }

            mesh_desc_dirty_ = true;
        }

        void Lights(PointLight const* lights, uint32_t num_lights)
        {
            num_lights_ = num_lights;

            gpu_system_.ReallocUploadMemBlock(light_mem_block_, num_lights * sizeof(LightBuffer));
            auto* light_mem = light_mem_block_.CpuAddress<LightBuffer>();
            for (uint32_t i = 0; i < num_lights; ++i)
            {
                light_mem[i] = EngineInternal::Buffer(lights[i]);
            }

            light_desc_dirty_ = true;
        }

        void Camera(GoldenSun::Camera const& camera)
        {
            camera_ = camera.Clone();
        }

        void Render(ID3D12GraphicsCommandList4* d3d12_cmd_list)
        {
            GpuCommandList cmd_list(d3d12_cmd_list);

            uint32_t const frame_index = gpu_system_.FrameIndex();

            acceleration_structure_.Build(cmd_list, frame_index);
            this->BuildDescHeap();

            d3d12_cmd_list->SetComputeRootSignature(ray_tracing_global_root_signature_.Get());

            {
                per_frame_constants_->camera_pos = XMFLOAT4(camera_.Eye().x, camera_.Eye().y, camera_.Eye().z, 1);

                auto const view =
                    XMMatrixLookAtLH(XMLoadFloat3(&camera_.Eye()), XMLoadFloat3(&camera_.LookAt()), XMLoadFloat3(&camera_.Up()));
                auto const proj = XMMatrixPerspectiveFovLH(camera_.Fov(), aspect_ratio_, camera_.NearPlane(), camera_.FarPlane());
                XMStoreFloat4x4(&per_frame_constants_->inv_view_proj, XMMatrixTranspose(XMMatrixInverse(nullptr, view * proj)));

                per_frame_constants_.UploadToGpu(frame_index);

                d3d12_cmd_list->SetComputeRootConstantBufferView(
                    std::to_underlying(GlobalRootSignature::Slot::SceneConstant), per_frame_constants_.GpuVirtualAddress(frame_index));
            }

            ID3D12DescriptorHeap* heaps[] = {reinterpret_cast<ID3D12DescriptorHeap*>(output_desc_block_.NativeDescriptorHeap())};
            d3d12_cmd_list->SetDescriptorHeaps(static_cast<uint32_t>(std::size(heaps)), heaps);
            d3d12_cmd_list->SetComputeRootDescriptorTable(std::to_underlying(GlobalRootSignature::Slot::OutputView),
                OffsetHandle(output_desc_block_.GpuHandle(), 0, descriptor_size_));
            d3d12_cmd_list->SetComputeRootShaderResourceView(std::to_underlying(GlobalRootSignature::Slot::AccelerationStructure),
                acceleration_structure_.TopLevelASBuffer().GpuVirtualAddress());
            d3d12_cmd_list->SetComputeRootDescriptorTable(std::to_underlying(GlobalRootSignature::Slot::MaterialBuffer),
                OffsetHandle(mesh_desc_block_.GpuHandle(), 0, descriptor_size_));
            d3d12_cmd_list->SetComputeRootDescriptorTable(std::to_underlying(GlobalRootSignature::Slot::LightBuffer),
                OffsetHandle(light_desc_block_.GpuHandle(), 0, descriptor_size_));

            D3D12_DISPATCH_RAYS_DESC dispatch_desc{};

            dispatch_desc.HitGroupTable.StartAddress = hit_group_shader_table_.GpuVirtualAddress();
            dispatch_desc.HitGroupTable.SizeInBytes = hit_group_shader_table_.Size();
            dispatch_desc.HitGroupTable.StrideInBytes = hit_group_shader_table_stride_;

            dispatch_desc.MissShaderTable.StartAddress = miss_shader_table_.GpuVirtualAddress();
            dispatch_desc.MissShaderTable.SizeInBytes = miss_shader_table_.Size();
            dispatch_desc.MissShaderTable.StrideInBytes = miss_shader_table_stride_;

            dispatch_desc.RayGenerationShaderRecord.StartAddress = ray_gen_shader_table_.GpuVirtualAddress();
            dispatch_desc.RayGenerationShaderRecord.SizeInBytes = ray_gen_shader_table_.Size();

            dispatch_desc.Width = width_;
            dispatch_desc.Height = height_;
            dispatch_desc.Depth = 1;

            d3d12_cmd_list->SetPipelineState1(state_obj_.Get());
            d3d12_cmd_list->DispatchRays(&dispatch_desc);
        }

        ID3D12Resource* Output() const noexcept
        {
            return reinterpret_cast<ID3D12Resource*>(ray_tracing_output_.NativeResource());
        }

    private:
        void CreateWindowSizeDependentResources()
        {
            ray_tracing_output_ = gpu_system_.CreateTexture2D(width_, height_, 1, format_, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"GoldenSun Output");

            output_desc_dirty_ = true;
        }

        void ReleaseWindowSizeDependentResources() noexcept
        {
            ray_tracing_output_.Reset();
            output_desc_dirty_ = true;
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

            auto* d3d12_device = reinterpret_cast<ID3D12Device5*>(gpu_system_.NativeDevice());

            ComPtr<ID3D12RootSignature> root_sig;
            TIFHR(d3d12_device->CreateRootSignature(
                1, blob->GetBufferPointer(), blob->GetBufferSize(), UuidOf<ID3D12RootSignature>(), root_sig.PutVoid()));
            return root_sig;
        }

        void CreateRootSignatures()
        {
            {
                D3D12_DESCRIPTOR_RANGE const ranges[] = {
                    {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}, // Output
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}, // Material
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}, // Light
                };

                D3D12_ROOT_PARAMETER const root_params[] = {
                    CreateRootParameterAsDescriptorTable(&ranges[0], 1),
                    CreateRootParameterAsShaderResourceView(0), // AccelerationStructure
                    CreateRootParameterAsConstantBufferView(0), // scene_cb
                    CreateRootParameterAsDescriptorTable(&ranges[1], 1),
                    CreateRootParameterAsDescriptorTable(&ranges[2], 1),
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
                    // VB and IB
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},
                    // albedo, metallic glossiness, emissive, normal, and occlusion
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 2, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},
                };

                D3D12_ROOT_PARAMETER const root_params[] = {
                    CreateRootParameterAsConstants(Align<sizeof(uint32_t)>(sizeof(PrimitiveConstantBuffer)) / sizeof(uint32_t), 0, 1),
                    CreateRootParameterAsDescriptorTable(&ranges[0], 1),
                    CreateRootParameterAsDescriptorTable(&ranges[1], 1),
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
            lib.DefineExport(c_any_hit_shader_name);
            lib.DefineExport(c_closest_hit_shader_name);
            for (auto const* miss_shader_name : c_miss_shader_names)
            {
                lib.DefineExport(miss_shader_name);
            }

            for (uint32_t ray_type = 0; ray_type < std::to_underlying(RayType::Num); ++ray_type)
            {
                auto& hit_group = ray_tracing_pipeline.CreateSubobject<D3D12HitGroupSubobject>();
                hit_group.SetAnyHitShaderImport(c_any_hit_shader_name);
                if (ray_type == std::to_underlying(RayType::Radiance))
                {
                    hit_group.SetClosestHitShaderImport(c_closest_hit_shader_name);
                }
                hit_group.SetHitGroupExport(c_hit_group_names[ray_type]);
                hit_group.SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
            }

            auto& shader_config = ray_tracing_pipeline.CreateSubobject<D3D12RayTracingShaderConfigSubobject>();
            uint32_t constexpr payload_size = std::max<uint32_t>(sizeof(RadianceRayPayload), sizeof(ShadowRayPayload));
            uint32_t constexpr attribute_size = sizeof(RayAttrib);
            shader_config.Config(payload_size, attribute_size);

            auto& local_root_signature = ray_tracing_pipeline.CreateSubobject<D3D12LocalRootSignatureSubobject>();
            local_root_signature.SetRootSignature(ray_tracing_local_root_signature_.Get());
            {
                auto& root_signature_association = ray_tracing_pipeline.CreateSubobject<D3D12SubobjectToExportsAssociationSubobject>();
                root_signature_association.SetSubobjectToAssociate(local_root_signature.Get());
                root_signature_association.AddExports(c_hit_group_names);
            }

            auto& global_root_signature = ray_tracing_pipeline.CreateSubobject<D3D12GlobalRootSignatureSubobject>();
            global_root_signature.SetRootSignature(ray_tracing_global_root_signature_.Get());

            auto& pipeline_config = ray_tracing_pipeline.CreateSubobject<D3D12RayTracingPipelineConfigSubobject>();
            pipeline_config.Config(MaxRayRecursionDepth);

            auto const& state_obj_desc = ray_tracing_pipeline.Retrieve();

#ifdef _DEBUG
            PrintStateObjectDesc(state_obj_desc);
#endif

            auto* d3d12_device = reinterpret_cast<ID3D12Device5*>(gpu_system_.NativeDevice());
            TIFHR(d3d12_device->CreateStateObject(&state_obj_desc, UuidOf<ID3D12StateObject>(), state_obj_.PutVoid()));
        }

        void BuildDescHeap()
        {
            uint32_t const num_primitives = primitive_start_.back();
            uint32_t const num_materials = material_start_.back();

            for (;;)
            {
                if (output_desc_dirty_)
                {
                    gpu_system_.ReallocCbvSrvUavDescBlock(output_desc_block_, 1);
                }
                if (mesh_desc_dirty_)
                {
                    // material buffer, vb and ib, material textures
                    gpu_system_.ReallocCbvSrvUavDescBlock(
                        mesh_desc_block_, 1 + num_primitives * 2 + num_materials * std::to_underlying(PbrMaterial::TextureSlot::Num));
                }
                if (light_desc_dirty_)
                {
                    gpu_system_.ReallocCbvSrvUavDescBlock(light_desc_block_, 1);
                }

                if ((output_desc_block_.NativeDescriptorHeap() != mesh_desc_block_.NativeDescriptorHeap()) ||
                    (output_desc_block_.NativeDescriptorHeap() != light_desc_block_.NativeDescriptorHeap()))
                {
                    output_desc_dirty_ = true;
                    mesh_desc_dirty_ = true;
                    light_desc_dirty_ = true;
                }
                else
                {
                    break;
                }
            }

            if (output_desc_dirty_)
            {
                gpu_system_.CreateUnorderedAccessView(
                    ray_tracing_output_, LinearFormatOf(format_), OffsetHandle(output_desc_block_.CpuHandle(), 0, descriptor_size_));
                output_desc_dirty_ = false;
            }

            if (mesh_desc_dirty_)
            {
                uint32_t slot = 0;

                gpu_system_.CreateShaderResourceView(
                    GpuBuffer(reinterpret_cast<ID3D12Resource*>(material_mem_block_.NativeResource()), D3D12_RESOURCE_STATE_GENERIC_READ),
                    material_mem_block_.Offset() / sizeof(PbrMaterialBuffer), num_materials, sizeof(PbrMaterialBuffer),
                    OffsetHandle(mesh_desc_block_.CpuHandle(), slot, descriptor_size_));
                ++slot;

                std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> buffer_gpu_handles;
                buffer_gpu_handles.resize(num_primitives);
                for (uint32_t i = 0; i < num_primitives; ++i)
                {
                    auto const& vb = mesh_buffers_[i * 2 + 0];
                    auto [vb_cpu_handle, vb_gpu_handle] =
                        OffsetHandle(mesh_desc_block_.CpuHandle(), mesh_desc_block_.GpuHandle(), slot + 0, descriptor_size_);
                    gpu_system_.CreateShaderResourceView(vb.buffer, 0, vb.num_elements, vb.stride, vb_cpu_handle);
                    buffer_gpu_handles[i] = std::move(vb_gpu_handle);

                    auto const& ib = mesh_buffers_[i * 2 + 1];
                    auto ib_cpu_handle = OffsetHandle(mesh_desc_block_.CpuHandle(), slot + 1, descriptor_size_);
                    gpu_system_.CreateShaderResourceView(ib.buffer, 0, ib.num_elements * ib.stride / 4, 0, ib_cpu_handle);

                    slot += 2;
                }

                std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> material_tex_gpu_handles;
                material_tex_gpu_handles.resize(num_materials);
                for (uint32_t i = 0; i < num_materials; ++i)
                {
                    uint32_t constexpr num_textures = std::to_underlying(PbrMaterial::TextureSlot::Num);
                    for (uint32_t j = 0; j < num_textures; ++j)
                    {
                        auto tex_cpu_handle = OffsetHandle(mesh_desc_block_.CpuHandle(), slot + j, descriptor_size_);
                        gpu_system_.CreateShaderResourceView(material_texs_[i * num_textures + j], tex_cpu_handle);
                    }

                    material_tex_gpu_handles[i] = OffsetHandle(mesh_desc_block_.GpuHandle(), slot + 0, descriptor_size_);

                    slot += num_textures;
                }

                void* ray_gen_shader_identifier;
                void* hit_group_shader_identifiers[std::to_underlying(RayType::Num)];
                void* miss_shader_identifiers[std::to_underlying(RayType::Num)];
                uint32_t shader_identifier_size;
                {
                    ComPtr<ID3D12StateObjectProperties> state_obj_properties = state_obj_.As<ID3D12StateObjectProperties>();
                    ray_gen_shader_identifier = state_obj_properties->GetShaderIdentifier(c_ray_gen_shader_name);
                    for (uint32_t i = 0; i < std::to_underlying(RayType::Num); i++)
                    {
                        miss_shader_identifiers[i] = state_obj_properties->GetShaderIdentifier(c_miss_shader_names[i]);
                    }
                    for (uint32_t i = 0; i < std::to_underlying(RayType::Num); i++)
                    {
                        hit_group_shader_identifiers[i] = state_obj_properties->GetShaderIdentifier(c_hit_group_names[i]);
                    }
                    shader_identifier_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
                }

                {
                    uint32_t const num_shader_records = 1;
                    uint32_t const shader_record_size = shader_identifier_size;
                    ShaderTable ray_gen_shader_table(gpu_system_, num_shader_records, shader_record_size, L"RayGenShaderTable");
                    ray_gen_shader_table.push_back(ShaderRecord(ray_gen_shader_identifier, shader_identifier_size));

                    ray_gen_shader_table.ExtractBuffer(ray_gen_shader_table_);
                }
                {
                    uint32_t constexpr num_shader_records = std::to_underlying(RayType::Num);
                    uint32_t const shader_record_size = shader_identifier_size;
                    ShaderTable miss_shader_table(gpu_system_, num_shader_records, shader_record_size, L"MissShaderTable");
                    for (auto* miss_shader_id : miss_shader_identifiers)
                    {
                        miss_shader_table.push_back(ShaderRecord(miss_shader_id, shader_identifier_size));
                    }

                    miss_shader_table_stride_ = miss_shader_table.ShaderRecordSize();
                    miss_shader_table.ExtractBuffer(miss_shader_table_);
                }

                {
                    uint32_t const num_shader_records = num_primitives * std::to_underlying(RayType::Num);
                    uint32_t const shader_record_size = shader_identifier_size + sizeof(LocalRootSignature::RootArguments);
                    ShaderTable hit_group_shader_table(gpu_system_, num_shader_records, shader_record_size, L"HitGroupShaderTable");

                    for (uint32_t i = 0; i < primitive_start_.size() - 1; ++i)
                    {
                        for (uint32_t j = 0; j < primitive_start_[i + 1] - primitive_start_[i]; ++j)
                        {
                            LocalRootSignature::RootArguments root_arguments;
                            root_arguments.cb.material_id = material_ids_[material_start_[i] + j];
                            root_arguments.buffer_gpu_handle = buffer_gpu_handles[primitive_start_[i] + j];
                            root_arguments.texture_gpu_handle = material_tex_gpu_handles[root_arguments.cb.material_id];

                            for (auto& hit_group_shader_identifier : hit_group_shader_identifiers)
                            {
                                hit_group_shader_table.push_back(ShaderRecord(
                                    hit_group_shader_identifier, shader_identifier_size, &root_arguments, sizeof(root_arguments)));
                            }
                        }
                    }

                    hit_group_shader_table_stride_ = hit_group_shader_table.ShaderRecordSize();
                    hit_group_shader_table.ExtractBuffer(hit_group_shader_table_);
                }

                mesh_desc_dirty_ = false;
            }

            if (light_desc_dirty_)
            {
                assert(light_mem_block_.Offset() / sizeof(LightBuffer) * sizeof(LightBuffer) == light_mem_block_.Offset());
                gpu_system_.CreateShaderResourceView(
                    GpuBuffer(reinterpret_cast<ID3D12Resource*>(light_mem_block_.NativeResource()), D3D12_RESOURCE_STATE_GENERIC_READ),
                    light_mem_block_.Offset() / sizeof(LightBuffer), num_lights_, sizeof(LightBuffer),
                    OffsetHandle(light_desc_block_.CpuHandle(), 0, descriptor_size_));

                light_desc_dirty_ = false;
            }
        }

    private:
        GpuSystem gpu_system_;
        ComPtr<ID3D12StateObject> state_obj_;

        uint32_t width_ = 0;
        uint32_t height_ = 0;
        float aspect_ratio_ = 0;
        DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;

        ConstantBuffer<SceneConstantBuffer> per_frame_constants_;

        ComPtr<ID3D12RootSignature> ray_tracing_global_root_signature_;
        ComPtr<ID3D12RootSignature> ray_tracing_local_root_signature_;

        uint32_t descriptor_size_;

        GpuDescriptorBlock output_desc_block_;
        GpuDescriptorBlock mesh_desc_block_;
        GpuDescriptorBlock light_desc_block_;

        bool output_desc_dirty_ = true;
        bool mesh_desc_dirty_ = true;
        bool light_desc_dirty_ = true;

        std::vector<uint32_t> primitive_start_;
        std::vector<uint32_t> material_start_;
        std::vector<uint32_t> material_ids_;
        uint32_t num_lights_ = 0;

        struct MeshBuffer
        {
            GpuBuffer buffer;
            uint32_t num_elements;
            uint32_t stride;
        };
        std::vector<MeshBuffer> mesh_buffers_;
        std::vector<GpuTexture2D> material_texs_;

        GpuMemoryBlock material_mem_block_;
        std::array<GpuTexture2D, std::to_underlying(PbrMaterial::TextureSlot::Num)> default_textures_;

        GpuMemoryBlock light_mem_block_;

        RaytracingAccelerationStructureManager acceleration_structure_;

        GpuTexture2D ray_tracing_output_;

        enum class RayType : uint32_t
        {
            Radiance = 0, // ~ Primary, reflected camera/view rays calculating color for each hit.
            Shadow,       // ~ Shadow/visibility rays, only testing for occlusion
            Num
        };

        static constexpr wchar_t const* c_ray_gen_shader_name = L"RayGenShader";
        static constexpr wchar_t const* c_hit_group_names[std::to_underlying(RayType::Num)] = {
            L"HitGroupRadianceRay", L"HitGroupShadowRay"};
        static constexpr wchar_t const* c_any_hit_shader_name = L"AnyHitShader";
        static constexpr wchar_t const* c_closest_hit_shader_name = L"ClosestHitShader";
        static constexpr wchar_t const* c_miss_shader_names[std::to_underlying(RayType::Num)] = {
            L"MissShaderRadianceRay", L"MissShaderShadowRay"};
        GpuUploadBuffer ray_gen_shader_table_;
        GpuUploadBuffer hit_group_shader_table_;
        uint32_t hit_group_shader_table_stride_ = 0xFFFFFFFFU;
        GpuUploadBuffer miss_shader_table_;
        uint32_t miss_shader_table_stride_ = 0xFFFFFFFFU;

        GoldenSun::Camera camera_;
    };


    Engine::Engine() = default;

    Engine::Engine(ID3D12Device5* device, ID3D12CommandQueue* cmd_queue) : impl_(new Impl(device, cmd_queue))
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

    void Engine::Meshes(Mesh const* meshes, uint32_t num_meshes)
    {
        return impl_->Meshes(meshes, num_meshes);
    }

    void Engine::Lights(PointLight const* lights, uint32_t num_lights)
    {
        return impl_->Lights(lights, num_lights);
    }

    void Engine::Camera(GoldenSun::Camera const& camera)
    {
        return impl_->Camera(camera);
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
