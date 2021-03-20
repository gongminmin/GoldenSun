#include "pch.hpp"

#include "MeshLoader.hpp"
#include "TextureLoader.hpp"

#include <filesystem>
#include <iostream>

#include <assimp/Importer.hpp>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <GoldenSun/Util.hpp>

using namespace GoldenSun;
using namespace DirectX;
using namespace std;

namespace
{
    XMFLOAT3 Color4ToFloat3(aiColor4D const& c)
    {
        return XMFLOAT3{c.r, c.g, c.b};
    }

    void ComputeNormal(std::vector<Vertex>& vertices, std::vector<Index>& indices) noexcept
    {
        std::vector<XMVECTOR> normals(vertices.size(), XMVectorZero());
        for (uint32_t i = 0; i < indices.size(); i += 3)
        {
            uint32_t const v0_index = indices[0];
            uint32_t const v1_index = indices[1];
            uint32_t const v2_index = indices[2];

            XMVECTOR v0 = XMLoadFloat3(&vertices[v0_index].position);
            XMVECTOR v1 = XMLoadFloat3(&vertices[v1_index].position);
            XMVECTOR v2 = XMLoadFloat3(&vertices[v2_index].position);

            XMVECTOR normal = XMVector3Cross(v1 - v0, v2 - v0);

            normals[v0_index] += normal;
            normals[v1_index] += normal;
            normals[v2_index] += normal;
        }

        for (size_t i = 0; i < vertices.size(); ++i)
        {
            XMStoreFloat3(&vertices[i].normal, XMVector3Normalize(normals[i]));
        }
    }

    void BuildNodeData(aiNode const* ai_node, XMMATRIX const& parent_to_world, std::vector<Mesh>& meshes)
    {
        auto const ai_transform = XMFLOAT4X4(&ai_node->mTransformation.a1);
        auto const local_to_parent = XMMatrixTranspose(XMLoadFloat4x4(&ai_transform));

        auto const transform_to_world = local_to_parent * parent_to_world;
        if (ai_node->mNumMeshes > 0)
        {
            for (uint32_t mi = 0; mi < ai_node->mNumMeshes; ++mi)
            {
                XMFLOAT4X4 world;
                XMStoreFloat4x4(&world, transform_to_world);
                meshes[ai_node->mMeshes[mi]].AddInstance(world);
            }
        }

        for (uint32_t ci = 0; ci < ai_node->mNumChildren; ++ci)
        {
            BuildNodeData(ai_node->mChildren[ci], transform_to_world, meshes);
        }
    }

    std::vector<PbrMaterial> BuildMaterials(
        ID3D12Device5* device, ID3D12GraphicsCommandList4* cmd_list, aiScene const* ai_scene, std::filesystem::path const& asset_path)
    {
        std::vector<PbrMaterial> materials;

        for (uint32_t mi = 0; mi < ai_scene->mNumMaterials; ++mi)
        {
            auto& material = materials.emplace_back();

            aiColor4D ai_albedo(0, 0, 0, 0);
            float ai_opacity = 1;
            float ai_metallic = 0;
            float ai_shininess = 1;
            aiColor4D ai_emissive(0, 0, 0, 0);
            int ai_two_sided = 0;
            float ai_alpha_test = 0;

            auto const* const mtl = ai_scene->mMaterials[mi];

            if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, &ai_albedo))
            {
                material.buffer.albedo = Color4ToFloat3(ai_albedo);
                material.buffer.opacity = ai_albedo.a;
            }
            else
            {
                if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &ai_albedo))
                {
                    material.buffer.albedo = Color4ToFloat3(ai_albedo);
                }
                if (AI_SUCCESS == aiGetMaterialFloat(mtl, AI_MATKEY_OPACITY, &ai_opacity))
                {
                    material.buffer.opacity = ai_opacity;
                }
            }

            if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &ai_emissive))
            {
                material.buffer.emissive = Color4ToFloat3(ai_emissive);
            }

            if (AI_SUCCESS == aiGetMaterialFloat(mtl, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, &ai_metallic))
            {
                material.buffer.metallic = ai_metallic;
            }

            if (AI_SUCCESS == aiGetMaterialFloat(mtl, AI_MATKEY_SHININESS, &ai_shininess))
            {
                material.buffer.glossiness = ai_shininess;
            }
            material.buffer.glossiness = std::max(1.0f, std::min(material.buffer.glossiness, MAX_GLOSSINESS));

            if ((material.buffer.opacity < 1) || (aiGetMaterialTextureCount(mtl, aiTextureType_OPACITY) > 0))
            {
                material.buffer.transparent = true;
            }

            if (AI_SUCCESS == aiGetMaterialInteger(mtl, AI_MATKEY_TWOSIDED, &ai_two_sided))
            {
                material.buffer.two_sided = ai_two_sided ? true : false;
            }

            aiString ai_alpha_mode;
            if (AI_SUCCESS == aiGetMaterialString(mtl, AI_MATKEY_GLTF_ALPHAMODE, &ai_alpha_mode))
            {
                if (strcmp(ai_alpha_mode.C_Str(), "MASK") == 0)
                {
                    if (AI_SUCCESS == aiGetMaterialFloat(mtl, AI_MATKEY_GLTF_ALPHACUTOFF, &ai_alpha_test))
                    {
                        material.buffer.alpha_test = ai_alpha_test;
                    }
                }
                else if (strcmp(ai_alpha_mode.C_Str(), "BLEND") == 0)
                {
                    material.buffer.transparent = true;
                }
                else if (strcmp(ai_alpha_mode.C_Str(), "OPAQUE") == 0)
                {
                    material.buffer.transparent = false;
                }
            }

            if (aiGetMaterialTextureCount(mtl, aiTextureType_DIFFUSE) > 0)
            {
                aiString str;
                aiGetMaterialTexture(mtl, aiTextureType_DIFFUSE, 0, &str, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                material.textures[ConvertToUint(PbrMaterial::TextureSlot::Albedo)] =
                    LoadTexture(device, cmd_list, (asset_path / str.C_Str()).string(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
            }

            if (aiGetMaterialTextureCount(mtl, aiTextureType_UNKNOWN) > 0)
            {
                aiString str;
                aiGetMaterialTexture(mtl, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &str, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr);
                material.textures[ConvertToUint(PbrMaterial::TextureSlot::MetallicGlossiness)] =
                    LoadTexture(device, cmd_list, (asset_path / str.C_Str()).string(), DXGI_FORMAT_R8G8B8A8_UNORM);
            }
            else if (aiGetMaterialTextureCount(mtl, aiTextureType_SHININESS) > 0)
            {
                aiString str;
                aiGetMaterialTexture(mtl, aiTextureType_SHININESS, 0, &str, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                material.textures[ConvertToUint(PbrMaterial::TextureSlot::MetallicGlossiness)] =
                    LoadTexture(device, cmd_list, (asset_path / str.C_Str()).string(), DXGI_FORMAT_R8G8B8A8_UNORM);
            }

            if (aiGetMaterialTextureCount(mtl, aiTextureType_EMISSIVE) > 0)
            {
                aiString str;
                aiGetMaterialTexture(mtl, aiTextureType_EMISSIVE, 0, &str, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                material.textures[ConvertToUint(PbrMaterial::TextureSlot::Emissive)] =
                    LoadTexture(device, cmd_list, (asset_path / str.C_Str()).string(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
            }

            if (aiGetMaterialTextureCount(mtl, aiTextureType_NORMALS) > 0)
            {
                aiString str;
                aiGetMaterialTexture(mtl, aiTextureType_NORMALS, 0, &str, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                material.textures[ConvertToUint(PbrMaterial::TextureSlot::Normal)] =
                    LoadTexture(device, cmd_list, (asset_path / str.C_Str()).string(), DXGI_FORMAT_R8G8B8A8_UNORM);

                aiGetMaterialFloat(mtl, AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_NORMALS, 0), &material.buffer.normal_scale);
            }

            if (aiGetMaterialTextureCount(mtl, aiTextureType_HEIGHT) > 0)
            {
                aiString str;
                aiGetMaterialTexture(mtl, aiTextureType_HEIGHT, 0, &str, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                material.textures[ConvertToUint(PbrMaterial::TextureSlot::Height)] =
                    LoadTexture(device, cmd_list, (asset_path / str.C_Str()).string(), DXGI_FORMAT_R8G8B8A8_UNORM);
            }

            if (aiGetMaterialTextureCount(mtl, aiTextureType_LIGHTMAP) > 0)
            {
                aiString str;
                aiGetMaterialTexture(mtl, aiTextureType_LIGHTMAP, 0, &str, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                material.textures[ConvertToUint(PbrMaterial::TextureSlot::Occlusion)] =
                    LoadTexture(device, cmd_list, (asset_path / str.C_Str()).string(), DXGI_FORMAT_R8G8B8A8_UNORM);

                aiGetMaterialFloat(mtl, AI_MATKEY_GLTF_TEXTURE_STRENGTH(aiTextureType_LIGHTMAP, 0), &material.buffer.occlusion_strength);
            }
        }

        return materials;
    }

    std::vector<Mesh> BuildMeshData(ID3D12Device5* device, aiScene const* ai_scene, std::vector<PbrMaterial> const& materials)
    {
        std::vector<Mesh> meshes;
        for (uint32_t mi = 0; mi < ai_scene->mNumMeshes; ++mi)
        {
            aiMesh const* ai_mesh = ai_scene->mMeshes[mi];

            std::wstring mesh_name_wide;
            {
                std::string const mesh_name = ai_mesh->mName.C_Str();
                size_t const wcs_len = mbstowcs(nullptr, mesh_name.data(), mesh_name.size());
                auto tmp = std::make_unique<wchar_t[]>(wcs_len + 1);
                mbstowcs(tmp.get(), mesh_name.data(), mesh_name.size());

                mesh_name_wide = tmp.get();
            }

            auto& new_mesh = meshes.emplace_back(DXGI_FORMAT_R32G32B32_FLOAT, static_cast<uint32_t>(sizeof(Vertex)), DXGI_FORMAT_R16_UINT,
                static_cast<uint32_t>(sizeof(uint16_t)));

            new_mesh.AddMaterial(materials[ai_mesh->mMaterialIndex]);

            std::vector<Index> indices;
            for (uint32_t fi = 0; fi < ai_mesh->mNumFaces; ++fi)
            {
                assert(ai_mesh->mFaces[fi].mNumIndices == 3);

                // TODO: Support 32-bit index
                for (uint32_t ii = 0; ii < ai_mesh->mFaces[fi].mNumIndices; ++ii)
                {
                    assert(ai_mesh->mFaces[fi].mIndices[ii] <= 0xFFFFU);
                }

                indices.push_back(static_cast<Index>(ai_mesh->mFaces[fi].mIndices[0]));
                indices.push_back(static_cast<Index>(ai_mesh->mFaces[fi].mIndices[1]));
                indices.push_back(static_cast<Index>(ai_mesh->mFaces[fi].mIndices[2]));
            }

            bool has_normal = (ai_mesh->mNormals != nullptr);
            bool has_texcoord = (ai_mesh->mTextureCoords[0] != nullptr);

            std::vector<Vertex> vertices(ai_mesh->mNumVertices);
            for (uint32_t vi = 0; vi < ai_mesh->mNumVertices; ++vi)
            {
                vertices[vi].position = XMFLOAT3(&ai_mesh->mVertices[vi].x);

                if (has_normal)
                {
                    vertices[vi].normal = XMFLOAT3(&ai_mesh->mNormals[vi].x);
                }

                if (has_texcoord)
                {
                    vertices[vi].tex_coord = XMFLOAT2(&ai_mesh->mTextureCoords[0][vi].x);
                }
            }

            if (!has_normal)
            {
                ComputeNormal(vertices, indices);

                has_normal = true;
            }

            D3D12_HEAP_PROPERTIES const upload_heap_prop = {
                D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};

            auto CreateUploadBuffer = [device, &upload_heap_prop](void const* data, uint32_t data_size, wchar_t const* name) {
                ComPtr<ID3D12Resource> ret;

                GpuUploadBuffer buffer(device, data_size, name);
                ret = buffer.Resource();

                memcpy(buffer.MappedData<void>(), data, data_size);

                return ret;
            };

            auto vb = CreateUploadBuffer(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(vertices[0])),
                (mesh_name_wide + L" Vertex Buffer").c_str());
            auto ib = CreateUploadBuffer(
                indices.data(), static_cast<uint32_t>(indices.size() * sizeof(indices[0])), (mesh_name_wide + L" Index Buffer").c_str());

            new_mesh.AddPrimitive(vb.Get(), ib.Get(), 0);
        }

        return meshes;
    }
} // namespace

namespace GoldenSun
{
    std::vector<Mesh> LoadMesh(ID3D12Device5* device, ID3D12GraphicsCommandList4* cmd_list, std::string_view file_name)
    {
        uint32_t const ppsteps = aiProcess_JoinIdenticalVertices      // join identical vertices/ optimize indexing
                                 | aiProcess_ValidateDataStructure    // perform a full validation of the loader's output
                                 | aiProcess_RemoveRedundantMaterials // remove redundant materials
                                 | aiProcess_FindInstances; // search for instanced meshes and remove them by references to one master

        Assimp::Importer importer;
        importer.SetPropertyInteger(AI_CONFIG_IMPORT_TER_MAKE_UVS, 1);
        importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80);
        importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, 0);
        importer.SetPropertyInteger(AI_CONFIG_GLOB_MEASURE_TIME, 1);

        aiScene const* ai_scene = importer.ReadFile(std::string(file_name).c_str(),
            ppsteps                             // configurable pp steps
                | aiProcess_GenSmoothNormals    // generate smooth normal vectors if not existing
                | aiProcess_Triangulate         // triangulate polygons with more than 3 edges
                | aiProcess_ConvertToLeftHanded // convert everything to D3D left handed space
            /*| aiProcess_FixInfacingNormals*/);

        std::vector<Mesh> meshes;
        if (ai_scene)
        {
            std::filesystem::path file_path = file_name;

            std::vector<PbrMaterial> materials = BuildMaterials(device, cmd_list, ai_scene, file_path.parent_path());
            meshes = BuildMeshData(device, ai_scene, materials);
            BuildNodeData(ai_scene->mRootNode, XMMatrixIdentity(), meshes);
        }
        else
        {
            std::cerr << "Assimp: Import file " << file_name << " error: " << importer.GetErrorString() << std::endl;
            Verify(false);
        }

        return meshes;
    }
} // namespace GoldenSun
