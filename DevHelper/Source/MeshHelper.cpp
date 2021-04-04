#include "pch.hpp"

#include <GoldenSun/MeshHelper.hpp>
#include <GoldenSun/TextureHelper.hpp>

#include <GoldenSun/Gpu/GpuSystem.hpp>
#include <GoldenSun/Util.hpp>

#include <filesystem>
#include <iostream>
#include <limits>

#include <assimp/Importer.hpp>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

using namespace GoldenSun;
using namespace DirectX;
using namespace std;

namespace
{
    XMFLOAT3 Color4ToFloat3(aiColor4D const& c)
    {
        return XMFLOAT3{c.r, c.g, c.b};
    }

    void ComputeNormal(std::vector<XMVECTOR> const& positions, std::vector<XMVECTOR>& normals, std::vector<Index> const& indices) noexcept
    {
        normals.assign(positions.size(), XMVectorZero());

        for (uint32_t i = 0; i < indices.size(); i += 3)
        {
            uint32_t const v0_index = indices[i + 0];
            uint32_t const v1_index = indices[i + 1];
            uint32_t const v2_index = indices[i + 2];

            XMVECTOR const& v0 = positions[v0_index];
            XMVECTOR const& v1 = positions[v1_index];
            XMVECTOR const& v2 = positions[v2_index];

            XMVECTOR const normal = XMVector3Cross(v1 - v0, v2 - v0);

            normals[v0_index] += normal;
            normals[v1_index] += normal;
            normals[v2_index] += normal;
        }

        for (auto& normal : normals)
        {
            normal = XMVectorSetW(XMVector3Normalize(normal), 0);
        }
    }

    void ComputeTangent(std::vector<XMVECTOR> const& positions, std::vector<XMVECTOR> const& normals,
        std::vector<XMVECTOR> const& tex_coords, std::vector<Index> const& indices, std::vector<XMVECTOR>& tangents,
        std::vector<XMVECTOR>& bitangents) noexcept
    {
        tangents.assign(positions.size(), XMVectorZero());
        bitangents.assign(positions.size(), XMVectorZero());

        for (uint32_t i = 0; i < indices.size(); i += 3)
        {
            uint32_t const v0_index = indices[i + 0];
            uint32_t const v1_index = indices[i + 1];
            uint32_t const v2_index = indices[i + 2];

            XMVECTOR const& v0 = positions[v0_index];
            XMVECTOR const& v1 = positions[v1_index];
            XMVECTOR const& v2 = positions[v2_index];

            XMVECTOR const v1v0 = v1 - v0;
            XMVECTOR const v2v0 = v2 - v0;

            XMVECTOR const& v0_tex = tex_coords[v0_index];
            XMVECTOR const& v1_tex = tex_coords[v1_index];
            XMVECTOR const& v2_tex = tex_coords[v2_index];

            XMVECTOR const st1 = v1_tex - v0_tex;
            XMVECTOR const st2 = v2_tex - v0_tex;

            float const denominator = XMVectorGetX(st1) * XMVectorGetY(st2) - XMVectorGetX(st2) * XMVectorGetY(st1);
            XMVECTOR tangent, bitangent;
            if (std::abs(denominator) < std::numeric_limits<float>::epsilon())
            {
                tangent = XMVectorSet(1, 0, 0, 0);
                bitangent = XMVectorSet(0, 1, 0, 0);
            }
            else
            {
                tangent = (XMVectorGetY(st2) * v1v0 - XMVectorGetY(st1) * v2v0) / denominator;
                bitangent = (XMVectorGetX(st1) * v2v0 - XMVectorGetX(st2) * v1v0) / denominator;
            }

            tangents[v0_index] += tangent;
            bitangents[v0_index] += bitangent;

            tangents[v1_index] += tangent;
            bitangents[v1_index] += bitangent;

            tangents[v2_index] += tangent;
            bitangents[v2_index] += bitangent;
        }

        for (size_t i = 0; i < positions.size(); ++i)
        {
            XMVECTOR tangent = tangents[i];
            XMVECTOR bitangent = bitangents[i];
            XMVECTOR const normal = normals[i];

            // Gram-Schmidt orthogonalize
            tangent = XMVector3Normalize(tangent - normal * XMVector3Dot(tangent, normal));
            tangent = XMVectorSetW(tangent, 0);
            tangents[i] = tangent;

            XMVECTOR bitangent_cross = XMVector3Cross(normal, tangent);
            // Calculate handedness
            if (XMVectorGetX(XMVector3Dot(bitangent_cross, bitangent)) < 0)
            {
                bitangent_cross = -bitangent_cross;
            }
            bitangent_cross = XMVectorSetW(bitangent_cross, 0);

            bitangents[i] = bitangent_cross;
        }
    }

    XMVECTOR ToQuaternion(XMVECTOR const& tangent, XMVECTOR const& bitangent, XMVECTOR const& normal) noexcept
    {
        float k = 1;
        if (XMVectorGetX(XMVector3Dot(bitangent, XMVector3Cross(normal, tangent))) < 0)
        {
            k = -1;
        }

        XMMATRIX const tangent_frame(tangent, k * bitangent, normal, XMVectorSet(0, 0, 0, 1));
        XMVECTOR tangent_quat = XMQuaternionRotationMatrix(tangent_frame);
        if (XMVectorGetW(tangent_quat) < 0)
        {
            tangent_quat = -tangent_quat;
        }
        if (k < 0)
        {
            tangent_quat = -tangent_quat;
        }

        return tangent_quat;
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
                MeshInstance instance;
                XMStoreFloat4x4(&instance.transform, transform_to_world);
                meshes[ai_node->mMeshes[mi]].AddInstance(std::move(instance));
            }
        }

        for (uint32_t ci = 0; ci < ai_node->mNumChildren; ++ci)
        {
            BuildNodeData(ai_node->mChildren[ci], transform_to_world, meshes);
        }
    }

    std::vector<PbrMaterial> BuildMaterials(GpuSystem& gpu_system, aiScene const* ai_scene, std::filesystem::path const& asset_path)
    {
        std::vector<PbrMaterial> materials;

        for (uint32_t mi = 0; mi < ai_scene->mNumMaterials; ++mi)
        {
            auto& material = materials.emplace_back();

            aiColor4D ai_albedo(0, 0, 0, 0);
            float ai_opacity = 1;
            float ai_metallic = 0;
            float ai_roughness = 1;
            aiColor4D ai_emissive(0, 0, 0, 0);
            int ai_two_sided = 0;
            float ai_alpha_cutoff = 0;
            float ai_normal_scale = 1;
            float ai_occlusion_strength = 1;

            auto const* const mtl = ai_scene->mMaterials[mi];

            if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, &ai_albedo))
            {
                material.Albedo() = Color4ToFloat3(ai_albedo);
                material.Opacity() = ai_albedo.a;
            }
            else
            {
                if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &ai_albedo))
                {
                    material.Albedo() = Color4ToFloat3(ai_albedo);
                }
                if (AI_SUCCESS == aiGetMaterialFloat(mtl, AI_MATKEY_OPACITY, &ai_opacity))
                {
                    material.Opacity() = ai_opacity;
                }
            }

            if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &ai_emissive))
            {
                material.Emissive() = Color4ToFloat3(ai_emissive);
            }

            if (AI_SUCCESS == aiGetMaterialFloat(mtl, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, &ai_metallic))
            {
                material.Metallic() = ai_metallic;
            }

            if (AI_SUCCESS == aiGetMaterialFloat(mtl, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, &ai_roughness))
            {
                material.Roughness() = ai_roughness;
            }
            else if (AI_SUCCESS == aiGetMaterialFloat(mtl, AI_MATKEY_SHININESS, &ai_roughness))
            {
                material.Roughness() =
                    1 - log(std::max(1.0f, std::min(ai_roughness, PbrMaterial::MaxGlossiness))) / log(PbrMaterial::MaxGlossiness);
            }
            else
            {
                material.Roughness() = 1;
            }

            if ((material.Opacity() < 1) || (aiGetMaterialTextureCount(mtl, aiTextureType_OPACITY) > 0))
            {
                material.Transparent() = true;
            }

            if (AI_SUCCESS == aiGetMaterialInteger(mtl, AI_MATKEY_TWOSIDED, &ai_two_sided))
            {
                material.TwoSided() = ai_two_sided ? true : false;
            }

            aiString ai_alpha_mode;
            if (AI_SUCCESS == aiGetMaterialString(mtl, AI_MATKEY_GLTF_ALPHAMODE, &ai_alpha_mode))
            {
                if (strcmp(ai_alpha_mode.C_Str(), "MASK") == 0)
                {
                    if (AI_SUCCESS == aiGetMaterialFloat(mtl, AI_MATKEY_GLTF_ALPHACUTOFF, &ai_alpha_cutoff))
                    {
                        material.AlphaCutoff() = ai_alpha_cutoff;
                    }
                }
                else if (strcmp(ai_alpha_mode.C_Str(), "BLEND") == 0)
                {
                    material.Transparent() = true;
                }
                else if (strcmp(ai_alpha_mode.C_Str(), "OPAQUE") == 0)
                {
                    material.Transparent() = false;
                }
            }

            if (aiGetMaterialTextureCount(mtl, aiTextureType_DIFFUSE) > 0)
            {
                aiString str;
                aiGetMaterialTexture(mtl, aiTextureType_DIFFUSE, 0, &str, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                auto texture = LoadTexture(gpu_system, (asset_path / str.C_Str()).string(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
                material.Texture(PbrMaterial::TextureSlot::Albedo, texture.NativeHandle<D3D12Traits>());
            }

            if (aiGetMaterialTextureCount(mtl, aiTextureType_UNKNOWN) > 0)
            {
                aiString str;
                aiGetMaterialTexture(mtl, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &str, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr);
                auto texture = LoadTexture(gpu_system, (asset_path / str.C_Str()).string(), DXGI_FORMAT_R8G8B8A8_UNORM);
                material.Texture(PbrMaterial::TextureSlot::MetallicRoughness, texture.NativeHandle<D3D12Traits>());
            }

            if (aiGetMaterialTextureCount(mtl, aiTextureType_EMISSIVE) > 0)
            {
                aiString str;
                aiGetMaterialTexture(mtl, aiTextureType_EMISSIVE, 0, &str, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                auto texture = LoadTexture(gpu_system, (asset_path / str.C_Str()).string(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
                material.Texture(PbrMaterial::TextureSlot::Emissive, texture.NativeHandle<D3D12Traits>());
            }

            if (aiGetMaterialTextureCount(mtl, aiTextureType_NORMALS) > 0)
            {
                aiString str;
                aiGetMaterialTexture(mtl, aiTextureType_NORMALS, 0, &str, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                auto texture = LoadTexture(gpu_system, (asset_path / str.C_Str()).string(), DXGI_FORMAT_R8G8B8A8_UNORM);
                material.Texture(PbrMaterial::TextureSlot::Normal, texture.NativeHandle<D3D12Traits>());

                aiGetMaterialFloat(mtl, AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_NORMALS, 0), &ai_normal_scale);
                material.NormalScale() = ai_normal_scale;
            }

            if (aiGetMaterialTextureCount(mtl, aiTextureType_LIGHTMAP) > 0)
            {
                aiString str;
                aiGetMaterialTexture(mtl, aiTextureType_LIGHTMAP, 0, &str, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                auto texture = LoadTexture(gpu_system, (asset_path / str.C_Str()).string(), DXGI_FORMAT_R8G8B8A8_UNORM);
                material.Texture(PbrMaterial::TextureSlot::Occlusion, texture.NativeHandle<D3D12Traits>());

                aiGetMaterialFloat(mtl, AI_MATKEY_GLTF_TEXTURE_STRENGTH(aiTextureType_LIGHTMAP, 0), &ai_occlusion_strength);
                material.OcclusionStrength() = ai_occlusion_strength;
            }
        }

        return materials;
    }

    std::vector<Mesh> BuildMeshData(GpuSystem& gpu_system, aiScene const* ai_scene, std::vector<PbrMaterial> const& materials)
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

                // TODO #16: Support 32-bit index
                for (uint32_t ii = 0; ii < ai_mesh->mFaces[fi].mNumIndices; ++ii)
                {
                    assert(ai_mesh->mFaces[fi].mIndices[ii] <= 0xFFFFU);
                }

                indices.push_back(static_cast<Index>(ai_mesh->mFaces[fi].mIndices[0]));
                indices.push_back(static_cast<Index>(ai_mesh->mFaces[fi].mIndices[1]));
                indices.push_back(static_cast<Index>(ai_mesh->mFaces[fi].mIndices[2]));
            }

            bool has_normal = (ai_mesh->mNormals != nullptr);
            bool has_tangent = (ai_mesh->mTangents != nullptr);
            bool has_bitangent = (ai_mesh->mBitangents != nullptr);
            bool has_texcoord = (ai_mesh->mTextureCoords[0] != nullptr);

            std::vector<XMVECTOR> positions(ai_mesh->mNumVertices);
            std::vector<XMVECTOR> normals(ai_mesh->mNumVertices);
            std::vector<XMVECTOR> tangents(ai_mesh->mNumVertices);
            std::vector<XMVECTOR> bitangents(ai_mesh->mNumVertices);
            std::vector<XMVECTOR> tex_coords(ai_mesh->mNumVertices);
            for (uint32_t vi = 0; vi < ai_mesh->mNumVertices; ++vi)
            {
                positions[vi] = XMLoadFloat3(reinterpret_cast<XMFLOAT3 const*>(&ai_mesh->mVertices[vi].x));

                if (has_normal)
                {
                    normals[vi] = XMLoadFloat3(reinterpret_cast<XMFLOAT3 const*>(&ai_mesh->mNormals[vi].x));
                }
                if (has_tangent)
                {
                    tangents[vi] = XMLoadFloat3(reinterpret_cast<XMFLOAT3 const*>(&ai_mesh->mTangents[vi].x));
                }
                if (has_bitangent)
                {
                    bitangents[vi] = XMLoadFloat3(reinterpret_cast<XMFLOAT3 const*>(&ai_mesh->mBitangents[vi].x));
                }

                if (has_texcoord)
                {
                    tex_coords[vi] = XMLoadFloat2(reinterpret_cast<XMFLOAT2 const*>(&ai_mesh->mTextureCoords[0][vi].x));
                }
            }

            if (!has_normal)
            {
                ComputeNormal(positions, normals, indices);
                has_normal = true;
            }

            if ((!has_tangent || !has_bitangent) && has_texcoord)
            {
                ComputeTangent(positions, normals, tex_coords, indices, tangents, bitangents);
                has_tangent = true;
            }

            std::vector<Vertex> vertices(ai_mesh->mNumVertices);
            for (uint32_t vi = 0; vi < ai_mesh->mNumVertices; ++vi)
            {
                XMStoreFloat3(&vertices[vi].position, positions[vi]);
                XMStoreFloat2(&vertices[vi].tex_coord, tex_coords[vi]);

                XMVECTOR const tangent_quat = ToQuaternion(tangents[vi], bitangents[vi], normals[vi]);
                XMStoreFloat4(&vertices[vi].tangent_quat, tangent_quat);
            }

            auto vb = gpu_system.CreateUploadBuffer(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(vertices[0])),
                (mesh_name_wide + L" Vertex Buffer").c_str());
            auto ib = gpu_system.CreateUploadBuffer(
                indices.data(), static_cast<uint32_t>(indices.size() * sizeof(indices[0])), (mesh_name_wide + L" Index Buffer").c_str());

            D3D12_RAYTRACING_GEOMETRY_FLAGS flags;
            if (materials[ai_mesh->mMaterialIndex].Transparent() || (materials[ai_mesh->mMaterialIndex].AlphaCutoff() > 0))
            {
                flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
            }
            else
            {
                flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
            }

            new_mesh.AddPrimitive(vb.NativeHandle<D3D12Traits>(), ib.NativeHandle<D3D12Traits>(), 0, flags);
        }

        return meshes;
    }
} // namespace

namespace GoldenSun
{
    std::vector<Mesh> LoadMesh(GpuSystem& gpu_system, std::string_view file_name)
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

            std::vector<PbrMaterial> materials = BuildMaterials(gpu_system, ai_scene, file_path.parent_path());
            meshes = BuildMeshData(gpu_system, ai_scene, materials);
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
