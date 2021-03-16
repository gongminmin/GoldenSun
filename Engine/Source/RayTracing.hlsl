struct SceneConstantBuffer
{
    float4x4 inv_view_proj;
    float4 camera_pos;

    float4 light_pos;
    float4 light_color;
};

struct Material
{
    float4 albedo;
};

struct Vertex
{
    float3 position;
    float3 normal;
};

struct PrimitiveConstantBuffer
{
    uint material_id;
};

RaytracingAccelerationStructure scene : register(t0, space0);
RWTexture2D<float4> render_target : register(u0, space0);

ConstantBuffer<SceneConstantBuffer> scene_cb : register(b0, space0);
StructuredBuffer<Material> material_buffer : register(t1, space0);

ConstantBuffer<PrimitiveConstantBuffer> primitive_cb : register(b0, space1);
StructuredBuffer<Vertex> vertex_buffer : register(t0, space1);
ByteAddressBuffer index_buffer : register(t1, space1);

uint3 Load3x16BitIndices(uint offset_bytes)
{
    uint const dword_aligned_offset = offset_bytes & ~3;
    uint2 const four_16bit_indices = index_buffer.Load2(dword_aligned_offset);

    uint3 ret;
    if (dword_aligned_offset == offset_bytes)
    {
        ret.x = four_16bit_indices.x & 0xFFFF;
        ret.y = (four_16bit_indices.x >> 16) & 0xFFFF;
        ret.z = four_16bit_indices.y & 0xFFFF;
    }
    else
    {
        ret.x = (four_16bit_indices.x >> 16) & 0xFFFF;
        ret.y = four_16bit_indices.y & 0xFFFF;
        ret.z = (four_16bit_indices.y >> 16) & 0xFFFF;
    }

    return ret;
}

float4 CalcLighting(float3 position, float3 normal)
{
    Material mtl = material_buffer[primitive_cb.material_id];

    float3 const light_dir = normalize(scene_cb.light_pos.xyz - position);
    float const n_dot_l = max(0.0f, dot(light_dir, normal));

    return float4(mtl.albedo.rgb * scene_cb.light_color.rgb * n_dot_l, mtl.albedo.a);
}

struct RayPayload
{
    float4 color;
};

[shader("raygeneration")]
void RayGenShader()
{
    float2 pos_ss = (DispatchRaysIndex().xy + 0.5f) / DispatchRaysDimensions().xy * 2 - 1;
    pos_ss.y = -pos_ss.y;

    float4 pos_ws = mul(float4(pos_ss, 0, 1), scene_cb.inv_view_proj);
    pos_ws /= pos_ws.w;

    RayDesc ray;
    ray.Origin = scene_cb.camera_pos.xyz;
    ray.Direction = normalize(pos_ws.xyz - ray.Origin);
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;

    RayPayload payload = {float4(0, 0, 0, 0)};
    TraceRay(scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

    render_target[DispatchRaysIndex().xy] = payload.color;
}

[shader("closesthit")]
void ClosestHitShader(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    float3 const hit_position = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

    uint const index_size = 2;
    uint const indices_per_triangle = 3;
    uint const triangle_index_stride = indices_per_triangle * index_size;
    uint const base_index = PrimitiveIndex() * triangle_index_stride;

    uint3 const indices = Load3x16BitIndices(base_index);

    float3 const vertex_normals[] = {vertex_buffer[indices[0]].normal, vertex_buffer[indices[1]].normal, vertex_buffer[indices[2]].normal};
    float3 const normal = vertex_normals[0] + attr.barycentrics.x * (vertex_normals[1] - vertex_normals[0]) +
                          attr.barycentrics.y * (vertex_normals[2] - vertex_normals[0]);

    float4 color = CalcLighting(hit_position, normal);

    float3 ambient = 0.1f;
    color.xyz += ambient;

    payload.color = color;
}

[shader("miss")]
void MissShader(inout RayPayload payload)
{
    float4 const background = float4(0.2f, 0.4f, 0.6f, 1.0f);
    payload.color = background;
}
