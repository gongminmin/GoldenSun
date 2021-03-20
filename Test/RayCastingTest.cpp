#include "GoldenSunTest.hpp"

#include <GoldenSun/GoldenSun.hpp>
#include <GoldenSun/Util.hpp>

#include <gtest/gtest.h>

using namespace DirectX;
using namespace GoldenSun;

class RayCastingTest : public testing::Test
{
public:
    void SetUp() override
    {
        auto& test_env = TestEnv();
        golden_sun_engine_ = std::make_unique<Engine>(test_env.Device(), test_env.CommandQueue());
    }

protected:
    std::unique_ptr<Engine> golden_sun_engine_;
};

TEST_F(RayCastingTest, SingleObject)
{
    auto& test_env = TestEnv();
    auto* device = test_env.Device();

    golden_sun_engine_->RenderTarget(1024, 768, DXGI_FORMAT_R8G8B8A8_UNORM);

    {
        XMFLOAT3 const eye = {2.0f, 2.0f, -5.0f};
        XMFLOAT3 const look_at = {0.0f, 0.0f, 0.0f};
        XMFLOAT3 const up = {0.0f, 1.0f, 0.0f};

        float const fov = XMConvertToRadians(45);
        float const near_plane = 0.1f;
        float far_plane = 20;

        golden_sun_engine_->Camera(eye, look_at, up, fov, near_plane, far_plane);
    }
    {
        XMFLOAT3 const light_pos = {-2.0f, 1.8f, -3.0f};
        XMFLOAT3 const light_color = {1.0f, 0.8f, 0.0f};
        XMFLOAT3 const light_falloff = {1, 0, 0};

        golden_sun_engine_->Light(light_pos, light_color, light_falloff);
    }

    float const v = 1 / sqrt(3.0f);
    Vertex const cube_vertices[] = {
        {{-1.0f, +1.0f, -1.0f}, {-v, +v, -v}},
        {{+1.0f, +1.0f, -1.0f}, {+v, +v, -v}},
        {{+1.0f, +1.0f, +1.0f}, {+v, +v, +v}},
        {{-1.0f, +1.0f, +1.0f}, {-v, +v, +v}},

        {{-1.0f, -1.0f, -1.0f}, {-v, -v, -v}},
        {{+1.0f, -1.0f, -1.0f}, {+v, -v, -v}},
        {{+1.0f, -1.0f, +1.0f}, {+v, -v, +v}},
        {{-1.0f, -1.0f, +1.0f}, {-v, -v, +v}},
    };

    Index const cube_indices[] = {
        3, 1, 0, 2, 1, 3, 6, 4, 5, 7, 4, 6, 3, 4, 7, 0, 4, 3, 1, 6, 5, 2, 6, 1, 0, 5, 4, 1, 5, 0, 2, 7, 6, 3, 7, 2};

    PbrMaterial mtl;
    mtl.buffer.albedo = {1.0f, 1.0f, 1.0f};

    D3D12_HEAP_PROPERTIES const upload_heap_prop = {
        D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};

    auto CreateUploadBuffer = [device, &upload_heap_prop](void const* data, uint32_t data_size, wchar_t const* name) {
        ComPtr<ID3D12Resource> ret;

        GpuUploadBuffer buffer(device, data_size, name);
        ret = buffer.Resource();

        memcpy(buffer.MappedData<void>(), data, data_size);

        return ret;
    };

    auto vb = CreateUploadBuffer(cube_vertices, sizeof(cube_vertices), L"Vertex Buffer");
    auto ib = CreateUploadBuffer(cube_indices, sizeof(cube_indices), L"Index Buffer");

    std::vector<Mesh> meshes;
    {
        auto& mesh = meshes.emplace_back(DXGI_FORMAT_R32G32B32_FLOAT, static_cast<uint32_t>(sizeof(Vertex)), DXGI_FORMAT_R16_UINT,
            static_cast<uint32_t>(sizeof(uint16_t)));
        mesh.AddMaterial(mtl);
        mesh.AddPrimitive(vb.Get(), ib.Get(), 0);

        XMFLOAT4X4 world;
        XMStoreFloat4x4(&world, XMMatrixIdentity());
        mesh.AddInstance(world);
    }

    test_env.BeginFrame();

    auto* cmd_list = test_env.CommandList();
    TIFHR(cmd_list->Reset(test_env.CommandAllocator(), nullptr));

    golden_sun_engine_->Geometries(cmd_list, meshes.data(), static_cast<uint32_t>(meshes.size()));

    golden_sun_engine_->Render(cmd_list);

    test_env.ExecuteCommandList();

    test_env.CompareWithExpected("RayCastingTest/SingleObject", golden_sun_engine_->Output());

    test_env.EndFrame();
}

TEST_F(RayCastingTest, MultipleObjects)
{
    auto& test_env = TestEnv();
    auto* device = test_env.Device();

    golden_sun_engine_->RenderTarget(1024, 768, DXGI_FORMAT_R8G8B8A8_UNORM);

    {
        XMFLOAT3 const eye = {2.0f, 2.0f, -5.0f};
        XMFLOAT3 const look_at = {0.0f, 0.0f, 0.0f};
        XMFLOAT3 const up = {0.0f, 1.0f, 0.0f};

        float const fov = XMConvertToRadians(45);
        float const near_plane = 0.1f;
        float far_plane = 20;

        golden_sun_engine_->Camera(eye, look_at, up, fov, near_plane, far_plane);
    }
    {
        XMFLOAT3 const light_pos = {2.0f, 1.8f, -3.0f};
        XMFLOAT3 const light_color = {1.0f, 0.8f, 0.0f};
        XMFLOAT3 const light_falloff = {1, 0, 0};

        golden_sun_engine_->Light(light_pos, light_color, light_falloff);
    }

    float const v = 1 / sqrt(3.0f);
    Vertex const cube_vertices[] = {
        {{-1.0f, +1.0f, -1.0f}, {-v, +v, -v}},
        {{+1.0f, +1.0f, -1.0f}, {+v, +v, -v}},
        {{+1.0f, +1.0f, +1.0f}, {+v, +v, +v}},
        {{-1.0f, +1.0f, +1.0f}, {-v, +v, +v}},

        {{-1.0f, -1.0f, -1.0f}, {-v, -v, -v}},
        {{+1.0f, -1.0f, -1.0f}, {+v, -v, -v}},
        {{+1.0f, -1.0f, +1.0f}, {+v, -v, +v}},
        {{-1.0f, -1.0f, +1.0f}, {-v, -v, +v}},
    };

    Index const cube_indices[] = {
        3, 1, 0, 2, 1, 3, 6, 4, 5, 7, 4, 6, 3, 4, 7, 0, 4, 3, 1, 6, 5, 2, 6, 1, 0, 5, 4, 1, 5, 0, 2, 7, 6, 3, 7, 2};

    float const sqrt3_2 = sqrt(3.0f) / 2;
    Vertex const tetrahedron_vertices[] = {
        {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{+sqrt3_2, 0.0f, -0.5f}, {sqrt3_2, 0.0f, -0.5f}},
        {{-sqrt3_2, 0.0f, -0.5f}, {sqrt3_2, 0.0f, -0.5f}},
    };

    Index const tetrahedron_indices[] = {0, 1, 2, 0, 3, 1, 0, 2, 3, 1, 3, 2};

    PbrMaterial mtls[2];
    mtls[0].buffer.albedo = {1.0f, 1.0f, 1.0f};
    mtls[1].buffer.albedo = {0.4f, 1.0f, 0.3f};

    D3D12_HEAP_PROPERTIES const upload_heap_prop = {
        D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};

    auto CreateUploadBuffer = [device, &upload_heap_prop](void const* data, uint32_t data_size, wchar_t const* name) {
        ComPtr<ID3D12Resource> ret;

        GpuUploadBuffer buffer(device, data_size, name);
        ret = buffer.Resource();

        memcpy(buffer.MappedData<void>(), data, data_size);

        return ret;
    };

    auto vb0 = CreateUploadBuffer(cube_vertices, sizeof(cube_vertices), L"Cube Vertex Buffer");
    auto ib0 = CreateUploadBuffer(cube_indices, sizeof(cube_indices), L"Cube Index Buffer");
    auto vb1 = CreateUploadBuffer(tetrahedron_vertices, sizeof(tetrahedron_vertices), L"Tetrahedron Vertex Buffer");
    auto ib1 = CreateUploadBuffer(tetrahedron_indices, sizeof(tetrahedron_indices), L"Tetrahedron Index Buffer");

    std::vector<Mesh> meshes;
    {
        auto& mesh0 = meshes.emplace_back(DXGI_FORMAT_R32G32B32_FLOAT, static_cast<uint32_t>(sizeof(Vertex)), DXGI_FORMAT_R16_UINT,
            static_cast<uint32_t>(sizeof(uint16_t)));
        mesh0.AddMaterial(mtls[0]);
        mesh0.AddPrimitive(vb0.Get(), ib0.Get(), 0);

        XMFLOAT4X4 world0;
        XMStoreFloat4x4(&world0, XMMatrixTranslation(-1.5f, 0, 0));
        mesh0.AddInstance(world0);

        auto& mesh1 = meshes.emplace_back(DXGI_FORMAT_R32G32B32_FLOAT, static_cast<uint32_t>(sizeof(Vertex)), DXGI_FORMAT_R16_UINT,
            static_cast<uint32_t>(sizeof(uint16_t)));
        mesh1.AddMaterial(mtls[1]);
        mesh1.AddPrimitive(vb1.Get(), ib1.Get(), 0);

        XMFLOAT4X4 world1;
        XMStoreFloat4x4(&world1, XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(+1.5f, 0, 0));
        mesh1.AddInstance(world1);
    }

    test_env.BeginFrame();

    auto* cmd_list = test_env.CommandList();
    TIFHR(cmd_list->Reset(test_env.CommandAllocator(), nullptr));

    golden_sun_engine_->Geometries(cmd_list, meshes.data(), static_cast<uint32_t>(meshes.size()));

    golden_sun_engine_->Render(cmd_list);

    test_env.ExecuteCommandList();

    test_env.CompareWithExpected("RayCastingTest/MultipleObjects", golden_sun_engine_->Output());

    test_env.EndFrame();
}

TEST_F(RayCastingTest, Instancing)
{
    auto& test_env = TestEnv();
    auto* device = test_env.Device();

    golden_sun_engine_->RenderTarget(1024, 768, DXGI_FORMAT_R8G8B8A8_UNORM);

    {
        XMFLOAT3 const eye = {2.0f, 2.0f, -5.0f};
        XMFLOAT3 const look_at = {0.0f, 0.0f, 0.0f};
        XMFLOAT3 const up = {0.0f, 1.0f, 0.0f};

        float const fov = XMConvertToRadians(45);
        float const near_plane = 0.1f;
        float far_plane = 20;

        golden_sun_engine_->Camera(eye, look_at, up, fov, near_plane, far_plane);
    }
    {
        XMFLOAT3 const light_pos = {2.0f, 1.8f, -3.0f};
        XMFLOAT3 const light_color = {1.0f, 0.8f, 0.0f};
        XMFLOAT3 const light_falloff = {1, 0, 0};

        golden_sun_engine_->Light(light_pos, light_color, light_falloff);
    }

    float const v = 1 / sqrt(3.0f);
    Vertex const cube_vertices[] = {
        {{-1.0f, +1.0f, -1.0f}, {-v, +v, -v}},
        {{+1.0f, +1.0f, -1.0f}, {+v, +v, -v}},
        {{+1.0f, +1.0f, +1.0f}, {+v, +v, +v}},
        {{-1.0f, +1.0f, +1.0f}, {-v, +v, +v}},

        {{-1.0f, -1.0f, -1.0f}, {-v, -v, -v}},
        {{+1.0f, -1.0f, -1.0f}, {+v, -v, -v}},
        {{+1.0f, -1.0f, +1.0f}, {+v, -v, +v}},
        {{-1.0f, -1.0f, +1.0f}, {-v, -v, +v}},
    };

    Index const cube_indices[] = {
        3, 1, 0, 2, 1, 3, 6, 4, 5, 7, 4, 6, 3, 4, 7, 0, 4, 3, 1, 6, 5, 2, 6, 1, 0, 5, 4, 1, 5, 0, 2, 7, 6, 3, 7, 2};

    float const sqrt3_2 = sqrt(3.0f) / 2;
    Vertex const tetrahedron_vertices[] = {
        {{0.0f, 2.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{0.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 1.0f}},
        {{+sqrt3_2 * 2, 0.0f, -1.0f}, {sqrt3_2, 0.0f, -0.5f}},
        {{-sqrt3_2 * 2, 0.0f, -1.0f}, {sqrt3_2, 0.0f, -0.5f}},
    };

    Index const tetrahedron_indices[] = {0, 1, 2, 0, 3, 1, 0, 2, 3, 1, 3, 2};

    PbrMaterial mtls[3];
    mtls[0].buffer.albedo = {1.0f, 1.0f, 1.0f};
    mtls[1].buffer.albedo = {0.4f, 1.0f, 0.3f};
    mtls[2].buffer.albedo = {0.8f, 0.4f, 0.6f};

    D3D12_HEAP_PROPERTIES const upload_heap_prop = {
        D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};

    auto CreateUploadBuffer = [device, &upload_heap_prop](void const* data, uint32_t data_size, wchar_t const* name) {
        ComPtr<ID3D12Resource> ret;

        GpuUploadBuffer buffer(device, data_size, name);
        ret = buffer.Resource();

        memcpy(buffer.MappedData<void>(), data, data_size);

        return ret;
    };

    auto vb0 = CreateUploadBuffer(cube_vertices, sizeof(cube_vertices), L"Cube Vertex Buffer");
    auto ib0 = CreateUploadBuffer(cube_indices, sizeof(cube_indices), L"Cube Index Buffer");
    auto vb1 = CreateUploadBuffer(tetrahedron_vertices, sizeof(tetrahedron_vertices), L"Tetrahedron Vertex Buffer");
    auto ib1 = CreateUploadBuffer(tetrahedron_indices, sizeof(tetrahedron_indices), L"Tetrahedron Index Buffer");

    std::vector<Mesh> meshes;
    {
        auto& mesh0 = meshes.emplace_back(DXGI_FORMAT_R32G32B32_FLOAT, static_cast<uint32_t>(sizeof(Vertex)), DXGI_FORMAT_R16_UINT,
            static_cast<uint32_t>(sizeof(uint16_t)));
        mesh0.AddMaterial(mtls[0]);
        mesh0.AddMaterial(mtls[1]);
        mesh0.AddPrimitive(vb0.Get(), ib0.Get(), 0);
        mesh0.AddPrimitive(vb1.Get(), ib1.Get(), 1);

        XMFLOAT4X4 world0;
        XMStoreFloat4x4(&world0, XMMatrixTranslation(-1.5f, 0, 0));
        mesh0.AddInstance(world0);

        XMStoreFloat4x4(&world0, XMMatrixScaling(0.4f, 0.4f, 0.4f) * XMMatrixRotationX(0.8f) * XMMatrixTranslation(-1.5f, 0, -1.8f));
        mesh0.AddInstance(world0);

        auto& mesh1 = meshes.emplace_back(DXGI_FORMAT_R32G32B32_FLOAT, static_cast<uint32_t>(sizeof(Vertex)), DXGI_FORMAT_R16_UINT,
            static_cast<uint32_t>(sizeof(uint16_t)));
        mesh1.AddMaterial(mtls[2]);
        mesh1.AddPrimitive(vb1.Get(), ib1.Get(), 0);

        XMFLOAT4X4 world1;
        XMStoreFloat4x4(&world1, XMMatrixScaling(0.75f, 0.75f, 0.75f) * XMMatrixTranslation(+1.5f, 0, 0));
        mesh1.AddInstance(world1);
    }

    test_env.BeginFrame();

    auto* cmd_list = test_env.CommandList();
    TIFHR(cmd_list->Reset(test_env.CommandAllocator(), nullptr));

    golden_sun_engine_->Geometries(cmd_list, meshes.data(), static_cast<uint32_t>(meshes.size()));

    golden_sun_engine_->Render(cmd_list);

    test_env.ExecuteCommandList();

    test_env.CompareWithExpected("RayCastingTest/Instancing", golden_sun_engine_->Output());

    test_env.EndFrame();
}
