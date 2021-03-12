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
        golden_sun_engine_ = CreateEngineD3D12(test_env.CommandQueue());
    }

protected:
    std::unique_ptr<Engine> golden_sun_engine_;
};

TEST_F(RayCastingTest, SimpleRayCasting)
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

        golden_sun_engine_->Light(light_pos, light_color);
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

    Material const cube_mtls[] = {{{1.0f, 1.0f, 1.0f, 1.0f}}};

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
    auto mb = CreateUploadBuffer(cube_mtls, sizeof(cube_mtls), L"Material Buffer");

    std::vector<std::unique_ptr<Mesh>> meshes;
    {
        auto& mesh = meshes.emplace_back(CreateMeshD3D12(DXGI_FORMAT_R32G32B32_FLOAT, static_cast<uint32_t>(sizeof(Vertex)),
            DXGI_FORMAT_R16_UINT, static_cast<uint32_t>(sizeof(uint16_t))));
        mesh->AddGeometry(vb.Get(), ib.Get(), 0);
    }

    golden_sun_engine_->Geometry(meshes.data(), static_cast<uint32_t>(meshes.size()), mb.Get(), 1);

    test_env.BeginFrame();

    auto* cmd_list = test_env.CommandList();
    TIFHR(cmd_list->Reset(test_env.CommandAllocator(), nullptr));

    golden_sun_engine_->Render(cmd_list);

    test_env.ExecuteCommandList();

    test_env.CompareWithExpected("SimpleRayCasting", golden_sun_engine_->Output());

    test_env.EndFrame();
}
