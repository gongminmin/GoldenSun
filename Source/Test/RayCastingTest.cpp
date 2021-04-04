#include "pch.hpp"

#include "GoldenSunTest.hpp"

#include <GoldenSun/MeshHelper.hpp>

using namespace DirectX;
using namespace GoldenSun;

class RayCastingTest : public testing::Test
{
public:
    void SetUp() override
    {
        auto& gpu_system = TestEnv().GpuSystem();

        auto* d3d12_device = gpu_system.NativeDeviceHandle<D3D12Traits>();
        auto* d3d12_cmd_queue = gpu_system.NativeCommandQueueHandle<D3D12Traits>();

        golden_sun_engine_ = Engine(d3d12_device, d3d12_cmd_queue);
    }

protected:
    Engine golden_sun_engine_;
};

TEST_F(RayCastingTest, SingleObject)
{
    auto& test_env = TestEnv();
    auto& gpu_system = test_env.GpuSystem();

    golden_sun_engine_.RenderTarget(1024, 768, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    {
        Camera camera;
        camera.Eye() = {2.0f, 2.0f, -5.0f};
        camera.LookAt() = {0.0f, 0.0f, 0.0f};
        camera.Up() = {0.0f, 1.0f, 0.0f};
        camera.Fov() = XMConvertToRadians(45);
        camera.NearPlane() = 0.1f;
        camera.FarPlane() = 20;

        golden_sun_engine_.Camera(camera);
    }
    {
        PointLight light;
        light.Position() = {-2.0f, 1.8f, -3.0f};
        light.Color() = {1.0f, 0.8f, 0.0f};
        light.Falloff() = {1, 0, 0};
        light.Shadowing() = false;

        golden_sun_engine_.Lights(&light, 1);
    }

    Vertex const cube_vertices[] = {
        {{-1.0f, +1.0f, -1.0f}, {+0.880476236f, +0.115916885f, -0.279848129f, -0.364705175f}, {-1.0f, +1.0f}},
        {{+1.0f, +1.0f, -1.0f}, {+0.880476236f, -0.115916885f, +0.279848129f, -0.364705175f}, {+1.0f, +1.0f}},
        {{+1.0f, +1.0f, +1.0f}, {-0.364705175f, +0.279848129f, -0.115916885f, +0.880476236f}, {+1.0f, +1.0f}},
        {{-1.0f, +1.0f, +1.0f}, {-0.364705175f, -0.279848129f, +0.115916885f, +0.880476236f}, {-1.0f, +1.0f}},

        {{-1.0f, -1.0f, -1.0f}, {-0.880476236f, +0.115916885f, +0.279848129f, -0.364705175f}, {-1.0f, -1.0f}},
        {{+1.0f, -1.0f, -1.0f}, {-0.880476236f, -0.115916885f, -0.279848129f, -0.364705175f}, {+1.0f, -1.0f}},
        {{+1.0f, -1.0f, +1.0f}, {+0.364705175f, +0.279848129f, +0.115916885f, +0.880476236f}, {+1.0f, -1.0f}},
        {{-1.0f, -1.0f, +1.0f}, {+0.364705175f, -0.279848129f, -0.115916885f, +0.880476236f}, {-1.0f, -1.0f}},
    };

    Index const cube_indices[] = {
        3, 1, 0, 2, 1, 3, 6, 4, 5, 7, 4, 6, 3, 4, 7, 0, 4, 3, 1, 6, 5, 2, 6, 1, 0, 5, 4, 1, 5, 0, 2, 7, 6, 3, 7, 2};

    PbrMaterial mtl;
    mtl.Albedo() = {1.0f, 1.0f, 1.0f};

    auto vb = gpu_system.CreateUploadBuffer(cube_vertices, sizeof(cube_vertices), L"Vertex Buffer");
    auto ib = gpu_system.CreateUploadBuffer(cube_indices, sizeof(cube_indices), L"Index Buffer");

    std::vector<Mesh> meshes;
    {
        auto& mesh = meshes.emplace_back(DXGI_FORMAT_R32G32B32_FLOAT, static_cast<uint32_t>(sizeof(Vertex)), DXGI_FORMAT_R16_UINT,
            static_cast<uint32_t>(sizeof(uint16_t)));
        mesh.AddMaterial(mtl);
        mesh.AddPrimitive(vb.NativeHandle<D3D12Traits>(), ib.NativeHandle<D3D12Traits>(), 0);

        MeshInstance instance;
        XMStoreFloat4x4(&instance.transform, XMMatrixIdentity());
        mesh.AddInstance(std::move(instance));
    }

    golden_sun_engine_.Meshes(meshes.data(), static_cast<uint32_t>(meshes.size()));

    auto cmd_list = gpu_system.CreateCommandList();
    golden_sun_engine_.Render(cmd_list.NativeHandle<D3D12Traits>());
    gpu_system.Execute(std::move(cmd_list));

    GpuTexture2D actual_image(golden_sun_engine_.Output(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    test_env.CompareWithExpected("RayCastingTest/SingleObject", actual_image);

    gpu_system.MoveToNextFrame();
}

TEST_F(RayCastingTest, MultipleObjects)
{
    auto& test_env = TestEnv();
    auto& gpu_system = test_env.GpuSystem();

    golden_sun_engine_.RenderTarget(1024, 768, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    {
        Camera camera;
        camera.Eye() = {2.0f, 2.0f, -5.0f};
        camera.LookAt() = {0.0f, 0.0f, 0.0f};
        camera.Up() = {0.0f, 1.0f, 0.0f};
        camera.Fov() = XMConvertToRadians(45);
        camera.NearPlane() = 0.1f;
        camera.FarPlane() = 20;

        golden_sun_engine_.Camera(camera);
    }
    {
        PointLight light;
        light.Position() = {2.0f, 1.8f, -3.0f};
        light.Color() = {1.0f, 0.8f, 0.0f};
        light.Falloff() = {1, 0, 0};
        light.Shadowing() = false;

        golden_sun_engine_.Lights(&light, 1);
    }

    Vertex const cube_vertices[] = {
        {{-1.0f, +1.0f, -1.0f}, {+0.880476236f, +0.115916885f, -0.279848129f, -0.364705175f}, {-1.0f, +1.0f}},
        {{+1.0f, +1.0f, -1.0f}, {+0.880476236f, -0.115916885f, +0.279848129f, -0.364705175f}, {+1.0f, +1.0f}},
        {{+1.0f, +1.0f, +1.0f}, {-0.364705175f, +0.279848129f, -0.115916885f, +0.880476236f}, {+1.0f, +1.0f}},
        {{-1.0f, +1.0f, +1.0f}, {-0.364705175f, -0.279848129f, +0.115916885f, +0.880476236f}, {-1.0f, +1.0f}},

        {{-1.0f, -1.0f, -1.0f}, {-0.880476236f, +0.115916885f, +0.279848129f, -0.364705175f}, {-1.0f, -1.0f}},
        {{+1.0f, -1.0f, -1.0f}, {-0.880476236f, -0.115916885f, -0.279848129f, -0.364705175f}, {+1.0f, -1.0f}},
        {{+1.0f, -1.0f, +1.0f}, {+0.364705175f, +0.279848129f, +0.115916885f, +0.880476236f}, {+1.0f, -1.0f}},
        {{-1.0f, -1.0f, +1.0f}, {+0.364705175f, -0.279848129f, -0.115916885f, +0.880476236f}, {-1.0f, -1.0f}},
    };

    Index const cube_indices[] = {
        3, 1, 0, 2, 1, 3, 6, 4, 5, 7, 4, 6, 3, 4, 7, 0, 4, 3, 1, 6, 5, 2, 6, 1, 0, 5, 4, 1, 5, 0, 2, 7, 6, 3, 7, 2};

    float const sqrt3_2 = sqrt(3.0f) / 2;
    Vertex const tetrahedron_vertices[] = {
        {{0.0f, 1.0f, 0.0f}, {-0.707106769f, 0, 0, 0.707106769f}, {0.0f, 1.0f}},
        {{0.0f, 0.0f, 1.0f}, {0, 0, 0, 1}, {0.0f, 0.0f}},
        {{+sqrt3_2, 0.0f, -0.5f}, {0, 0.785187602f, 0, 0.619257927f}, {+sqrt3_2, 0.0f}},
        {{-sqrt3_2, 0.0f, -0.5f}, {-0.866025448f, 0, -0.5f, 0}, {-sqrt3_2, 0.0f}},
    };

    Index const tetrahedron_indices[] = {0, 1, 2, 0, 3, 1, 0, 2, 3, 1, 3, 2};

    PbrMaterial mtls[2];
    mtls[0].Albedo() = {1.0f, 1.0f, 1.0f};
    mtls[1].Albedo() = {0.4f, 1.0f, 0.3f};

    auto vb0 = gpu_system.CreateUploadBuffer(cube_vertices, sizeof(cube_vertices), L"Cube Vertex Buffer");
    auto ib0 = gpu_system.CreateUploadBuffer(cube_indices, sizeof(cube_indices), L"Cube Index Buffer");
    auto vb1 = gpu_system.CreateUploadBuffer(tetrahedron_vertices, sizeof(tetrahedron_vertices), L"Tetrahedron Vertex Buffer");
    auto ib1 = gpu_system.CreateUploadBuffer(tetrahedron_indices, sizeof(tetrahedron_indices), L"Tetrahedron Index Buffer");

    std::vector<Mesh> meshes;
    {
        auto& mesh0 = meshes.emplace_back(DXGI_FORMAT_R32G32B32_FLOAT, static_cast<uint32_t>(sizeof(Vertex)), DXGI_FORMAT_R16_UINT,
            static_cast<uint32_t>(sizeof(uint16_t)));
        mesh0.AddMaterial(mtls[0]);
        mesh0.AddPrimitive(vb0.NativeHandle<D3D12Traits>(), ib0.NativeHandle<D3D12Traits>(), 0);

        MeshInstance instance0;
        XMStoreFloat4x4(&instance0.transform, XMMatrixTranslation(-1.5f, 0, 0));
        mesh0.AddInstance(std::move(instance0));

        auto& mesh1 = meshes.emplace_back(DXGI_FORMAT_R32G32B32_FLOAT, static_cast<uint32_t>(sizeof(Vertex)), DXGI_FORMAT_R16_UINT,
            static_cast<uint32_t>(sizeof(uint16_t)));
        mesh1.AddMaterial(mtls[1]);
        mesh1.AddPrimitive(vb1.NativeHandle<D3D12Traits>(), ib1.NativeHandle<D3D12Traits>(), 0);

        MeshInstance instance1;
        XMStoreFloat4x4(&instance1.transform, XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(+1.5f, 0, 0));
        mesh1.AddInstance(instance1);
    }

    golden_sun_engine_.Meshes(meshes.data(), static_cast<uint32_t>(meshes.size()));

    auto cmd_list = gpu_system.CreateCommandList();
    golden_sun_engine_.Render(cmd_list.NativeHandle<D3D12Traits>());
    gpu_system.Execute(std::move(cmd_list));

    GpuTexture2D actual_image(golden_sun_engine_.Output(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    test_env.CompareWithExpected("RayCastingTest/MultipleObjects", actual_image);

    gpu_system.MoveToNextFrame();
}

TEST_F(RayCastingTest, Instancing)
{
    auto& test_env = TestEnv();
    auto& gpu_system = test_env.GpuSystem();

    golden_sun_engine_.RenderTarget(1024, 768, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    {
        Camera camera;
        camera.Eye() = {2.0f, 2.0f, -5.0f};
        camera.LookAt() = {0.0f, 0.0f, 0.0f};
        camera.Up() = {0.0f, 1.0f, 0.0f};
        camera.Fov() = XMConvertToRadians(45);
        camera.NearPlane() = 0.1f;
        camera.FarPlane() = 20;

        golden_sun_engine_.Camera(camera);
    }
    {
        PointLight light;
        light.Position() = {2.0f, 1.8f, -3.0f};
        light.Color() = {1.0f, 0.8f, 0.0f};
        light.Falloff() = {1, 0, 0};
        light.Shadowing() = false;

        golden_sun_engine_.Lights(&light, 1);
    }

    Vertex const cube_vertices[] = {
        {{-1.0f, +1.0f, -1.0f}, {+0.880476236f, +0.115916885f, -0.279848129f, -0.364705175f}, {-1.0f, +1.0f}},
        {{+1.0f, +1.0f, -1.0f}, {+0.880476236f, -0.115916885f, +0.279848129f, -0.364705175f}, {+1.0f, +1.0f}},
        {{+1.0f, +1.0f, +1.0f}, {-0.364705175f, +0.279848129f, -0.115916885f, +0.880476236f}, {+1.0f, +1.0f}},
        {{-1.0f, +1.0f, +1.0f}, {-0.364705175f, -0.279848129f, +0.115916885f, +0.880476236f}, {-1.0f, +1.0f}},

        {{-1.0f, -1.0f, -1.0f}, {-0.880476236f, +0.115916885f, +0.279848129f, -0.364705175f}, {-1.0f, -1.0f}},
        {{+1.0f, -1.0f, -1.0f}, {-0.880476236f, -0.115916885f, -0.279848129f, -0.364705175f}, {+1.0f, -1.0f}},
        {{+1.0f, -1.0f, +1.0f}, {+0.364705175f, +0.279848129f, +0.115916885f, +0.880476236f}, {+1.0f, -1.0f}},
        {{-1.0f, -1.0f, +1.0f}, {+0.364705175f, -0.279848129f, -0.115916885f, +0.880476236f}, {-1.0f, -1.0f}},
    };

    Index const cube_indices[] = {
        3, 1, 0, 2, 1, 3, 6, 4, 5, 7, 4, 6, 3, 4, 7, 0, 4, 3, 1, 6, 5, 2, 6, 1, 0, 5, 4, 1, 5, 0, 2, 7, 6, 3, 7, 2};

    float const sqrt3_2 = sqrt(3.0f) / 2;
    Vertex const tetrahedron_vertices[] = {
        {{0.0f, 2.0f, 0.0f}, {-0.707106769f, 0, 0, 0.707106769f}, {0.0f, 1.0f}},
        {{0.0f, 0.0f, 2.0f}, {0, 0, 0, 1}, {0.0f, 0.0f}},
        {{+sqrt3_2 * 2, 0.0f, -1.0f}, {0, 0.785187602f, 0, 0.619257927f}, {+sqrt3_2, 0.0f}},
        {{-sqrt3_2 * 2, 0.0f, -1.0f}, {-0.866025448f, 0, -0.5f, 0}, {-sqrt3_2, 0.0f}},
    };

    Index const tetrahedron_indices[] = {0, 1, 2, 0, 3, 1, 0, 2, 3, 1, 3, 2};

    PbrMaterial mtls[3];
    mtls[0].Albedo() = {1.0f, 1.0f, 1.0f};
    mtls[1].Albedo() = {0.4f, 1.0f, 0.3f};
    mtls[2].Albedo() = {0.8f, 0.4f, 0.6f};

    auto vb0 = gpu_system.CreateUploadBuffer(cube_vertices, sizeof(cube_vertices), L"Cube Vertex Buffer");
    auto ib0 = gpu_system.CreateUploadBuffer(cube_indices, sizeof(cube_indices), L"Cube Index Buffer");
    auto vb1 = gpu_system.CreateUploadBuffer(tetrahedron_vertices, sizeof(tetrahedron_vertices), L"Tetrahedron Vertex Buffer");
    auto ib1 = gpu_system.CreateUploadBuffer(tetrahedron_indices, sizeof(tetrahedron_indices), L"Tetrahedron Index Buffer");

    std::vector<Mesh> meshes;
    {
        auto& mesh0 = meshes.emplace_back(DXGI_FORMAT_R32G32B32_FLOAT, static_cast<uint32_t>(sizeof(Vertex)), DXGI_FORMAT_R16_UINT,
            static_cast<uint32_t>(sizeof(uint16_t)));
        mesh0.AddMaterial(mtls[0]);
        mesh0.AddMaterial(mtls[1]);
        mesh0.AddPrimitive(vb0.NativeHandle<D3D12Traits>(), ib0.NativeHandle<D3D12Traits>(), 0);
        mesh0.AddPrimitive(vb1.NativeHandle<D3D12Traits>(), ib1.NativeHandle<D3D12Traits>(), 1);

        MeshInstance instance0;
        XMStoreFloat4x4(&instance0.transform, XMMatrixTranslation(-1.5f, 0, 0));
        mesh0.AddInstance(std::move(instance0));

        XMStoreFloat4x4(
            &instance0.transform, XMMatrixScaling(0.4f, 0.4f, 0.4f) * XMMatrixRotationX(0.8f) * XMMatrixTranslation(-1.5f, 0, -1.8f));
        mesh0.AddInstance(std::move(instance0));

        auto& mesh1 = meshes.emplace_back(DXGI_FORMAT_R32G32B32_FLOAT, static_cast<uint32_t>(sizeof(Vertex)), DXGI_FORMAT_R16_UINT,
            static_cast<uint32_t>(sizeof(uint16_t)));
        mesh1.AddMaterial(mtls[2]);
        mesh1.AddPrimitive(vb1.NativeHandle<D3D12Traits>(), ib1.NativeHandle<D3D12Traits>(), 0);

        MeshInstance instance1;
        XMStoreFloat4x4(&instance1.transform, XMMatrixScaling(0.75f, 0.75f, 0.75f) * XMMatrixTranslation(+1.5f, 0, 0));
        mesh1.AddInstance(std::move(instance1));
    }

    golden_sun_engine_.Meshes(meshes.data(), static_cast<uint32_t>(meshes.size()));

    auto cmd_list = gpu_system.CreateCommandList();
    golden_sun_engine_.Render(cmd_list.NativeHandle<D3D12Traits>());
    gpu_system.Execute(std::move(cmd_list));

    GpuTexture2D actual_image(golden_sun_engine_.Output(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    test_env.CompareWithExpected("RayCastingTest/Instancing", actual_image);

    gpu_system.MoveToNextFrame();
}

TEST_F(RayCastingTest, Mesh)
{
    auto& test_env = TestEnv();
    auto& gpu_system = test_env.GpuSystem();

    golden_sun_engine_.RenderTarget(1024, 768, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    {
        Camera camera;
        camera.Eye() = {2.0f, 2.0f, -5.0f};
        camera.LookAt() = {0.0f, 0.0f, 0.0f};
        camera.Up() = {0.0f, 1.0f, 0.0f};
        camera.Fov() = XMConvertToRadians(45);
        camera.NearPlane() = 0.1f;
        camera.FarPlane() = 20;

        golden_sun_engine_.Camera(camera);
    }
    {
        PointLight light;
        light.Position() = {2.0f, 0.0f, -2.0f};
        light.Color() = {15.0f, 18.0f, 15.0f};
        light.Falloff() = {1, 0, 1};
        light.Shadowing() = false;

        golden_sun_engine_.Lights(&light, 1);
    }

    auto meshes = LoadMesh(gpu_system, test_env.AssetDir() + "DamagedHelmet/DamagedHelmet.gltf");
    for (auto& mesh : meshes)
    {
        MeshInstance instance;
        XMStoreFloat4x4(&instance.transform,
            XMLoadFloat4x4(&mesh.Instance(0).transform) * XMMatrixRotationY(0.4f) * XMMatrixTranslation(-1.8f, 0.5f, 0));
        mesh.AddInstance(std::move(instance));

        XMStoreFloat4x4(&instance.transform, XMLoadFloat4x4(&mesh.Instance(0).transform) * XMMatrixScaling(0.8f, 0.8f, 0.8f) *
                                                 XMMatrixRotationY(-0.8f) * XMMatrixTranslation(+1.8f, 0, 0));
        mesh.AddInstance(std::move(instance));
    }
    golden_sun_engine_.Meshes(meshes.data(), static_cast<uint32_t>(meshes.size()));

    auto cmd_list = gpu_system.CreateCommandList();
    golden_sun_engine_.Render(cmd_list.NativeHandle<D3D12Traits>());
    gpu_system.Execute(std::move(cmd_list));

    GpuTexture2D actual_image(golden_sun_engine_.Output(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    test_env.CompareWithExpected("RayCastingTest/Mesh", actual_image);

    gpu_system.MoveToNextFrame();
}

TEST_F(RayCastingTest, MeshShadowed)
{
    auto& test_env = TestEnv();
    auto& gpu_system = test_env.GpuSystem();

    golden_sun_engine_.RenderTarget(1024, 768, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    {
        Camera camera;
        camera.Eye() = {2.0f, 2.0f, -5.0f};
        camera.LookAt() = {0.0f, 0.0f, 0.0f};
        camera.Up() = {0.0f, 1.0f, 0.0f};
        camera.Fov() = XMConvertToRadians(45);
        camera.NearPlane() = 0.1f;
        camera.FarPlane() = 20;

        golden_sun_engine_.Camera(camera);
    }
    {
        std::vector<PointLight> lights;

        auto& light0 = lights.emplace_back();
        light0.Position() = {2.0f, 0.0f, -2.0f};
        light0.Color() = {15.0f, 18.0f, 15.0f};
        light0.Falloff() = {1, 0, 1};
        light0.Shadowing() = true;

        auto& light1 = lights.emplace_back();
        light1.Position() = {-2.0f, 1.8f, -3.0f};
        light1.Color() = {15.0f, 4.5f, 4.5f};
        light1.Falloff() = {1, 0, 1};
        light1.Shadowing() = false;

        golden_sun_engine_.Lights(lights.data(), static_cast<uint32_t>(lights.size()));
    }

    auto meshes = LoadMesh(gpu_system, test_env.AssetDir() + "DamagedHelmet/DamagedHelmet.gltf");
    for (auto& mesh : meshes)
    {
        MeshInstance instance;
        XMStoreFloat4x4(&instance.transform,
            XMLoadFloat4x4(&mesh.Instance(0).transform) * XMMatrixRotationY(0.4f) * XMMatrixTranslation(-1.8f, 0.5f, 0));
        mesh.AddInstance(std::move(instance));

        XMStoreFloat4x4(&instance.transform, XMLoadFloat4x4(&mesh.Instance(0).transform) * XMMatrixScaling(0.8f, 0.8f, 0.8f) *
                                                 XMMatrixRotationY(-0.8f) * XMMatrixTranslation(+1.8f, 0, 0));
        mesh.AddInstance(std::move(instance));
    }
    golden_sun_engine_.Meshes(meshes.data(), static_cast<uint32_t>(meshes.size()));

    auto cmd_list = gpu_system.CreateCommandList();
    golden_sun_engine_.Render(cmd_list.NativeHandle<D3D12Traits>());
    gpu_system.Execute(std::move(cmd_list));

    GpuTexture2D actual_image(golden_sun_engine_.Output(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    test_env.CompareWithExpected("RayCastingTest/MeshShadowed", actual_image);

    gpu_system.MoveToNextFrame();
}

TEST_F(RayCastingTest, Transparent)
{
    auto& test_env = TestEnv();
    auto& gpu_system = test_env.GpuSystem();

    golden_sun_engine_.RenderTarget(1024, 768, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    {
        Camera camera;
        camera.Eye() = {0.0f, 1.0f, -5.0f};
        camera.LookAt() = {0.0f, 0.5f, 0.0f};
        camera.Up() = {0.0f, 1.0f, 0.0f};
        camera.Fov() = XMConvertToRadians(45);
        camera.NearPlane() = 0.1f;
        camera.FarPlane() = 20;

        golden_sun_engine_.Camera(camera);
    }
    {
        std::vector<PointLight> lights;

        auto& light0 = lights.emplace_back();
        light0.Position() = {2.0f, 0.0f, -2.0f};
        light0.Color() = {8.0f, 10.0f, 8.0f};
        light0.Falloff() = {1, 0, 1};
        light0.Shadowing() = true;

        auto& light1 = lights.emplace_back();
        light1.Position() = {-2.0f, 1.5f, -3.0f};
        light1.Color() = {12.0f, 2.5f, 2.5f};
        light1.Falloff() = {1, 0, 1};
        light1.Shadowing() = true;

        golden_sun_engine_.Lights(lights.data(), static_cast<uint32_t>(lights.size()));
    }

    auto meshes = LoadMesh(gpu_system, test_env.AssetDir() + "AlphaBlendModeTest/AlphaBlendModeTest.gltf");
    for (auto& mesh : meshes)
    {
        XMStoreFloat4x4(&mesh.Instance(0).transform, XMLoadFloat4x4(&mesh.Instance(0).transform) * XMMatrixScaling(0.6f, 0.6f, 0.6f));
    }
    golden_sun_engine_.Meshes(meshes.data(), static_cast<uint32_t>(meshes.size()));

    auto cmd_list = gpu_system.CreateCommandList();
    golden_sun_engine_.Render(cmd_list.NativeHandle<D3D12Traits>());
    gpu_system.Execute(std::move(cmd_list));

    GpuTexture2D actual_image(golden_sun_engine_.Output(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    test_env.CompareWithExpected("RayCastingTest/Transparent", actual_image);

    gpu_system.MoveToNextFrame();
}
