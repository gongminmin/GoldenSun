#define TILE_DIM 16

cbuffer param_cb : register(b0)
{
    uint2 width_height;
    float channel_tolerance;
};

Texture2D<float4> expected_image : register(t0);
Texture2D<float4> actual_image : register(t1);

RWTexture2D<float4> result_image : register(u0);
RWStructuredBuffer<uint4> error_buffer : register(u1);

groupshared float4 diff_sh[TILE_DIM * TILE_DIM];

[numthreads(TILE_DIM, TILE_DIM, 1)]
void CompareImagesCS(uint3 thread_id : SV_DispatchThreadID, uint group_index : SV_GroupIndex)
{
    float4 diff = 0;
    if (all(thread_id.xy < width_height))
    {
        float4 const expected_texel = expected_image.Load(uint3(thread_id.xy, 0));
        float4 const actual_texel = actual_image.Load(uint3(thread_id.xy, 0));

        diff = abs(expected_texel - actual_texel);
        for (uint i = 0; i < 4; ++i)
        {
            if (diff[i] < channel_tolerance)
            {
                diff[i] = 0;
            }
        }
    }

    result_image[thread_id.xy] = diff;

    diff_sh[group_index] = diff;
    GroupMemoryBarrierWithGroupSync();

    for (uint offset = (TILE_DIM * TILE_DIM) >> 1; offset > 32; offset >>= 1)
    {
        if (group_index < offset)
        {
            diff_sh[group_index] += diff_sh[group_index + offset];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    for (offset = 32; offset > 0; offset >>= 1)
    {
        if (group_index < offset)
        {
            diff_sh[group_index] += diff_sh[group_index + offset];
        }
    }

    if (0 == group_index)
    {
        uint4 const idiff = uint4(diff_sh[0] * 255 + 0.5f);
        InterlockedAdd(error_buffer[0].x, idiff.x);
        InterlockedAdd(error_buffer[0].y, idiff.y);
        InterlockedAdd(error_buffer[0].z, idiff.z);
        InterlockedAdd(error_buffer[0].w, idiff.w);
    }
}