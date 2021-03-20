struct SceneConstantBuffer
{
    float4x4 inv_view_proj;
    float4 camera_pos;

    float4 light_pos;
    float4 light_color;
    float4 light_falloff;
};

struct PbrMaterial
{
    float3 albedo;
    float opacity;
    float3 emissive;
    float metallic;
    float glossiness;
    float alpha_test;
    float normal_scale;
    float occlusion_strength;
    bool transparent;
    bool two_sided;
    uint2 paddings;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float2 tex_coord;
};

struct PrimitiveConstantBuffer
{
    uint material_id;
};

RaytracingAccelerationStructure scene : register(t0, space0);
RWTexture2D<float4> render_target : register(u0, space0);

ConstantBuffer<SceneConstantBuffer> scene_cb : register(b0, space0);
StructuredBuffer<PbrMaterial> material_buffer : register(t1, space0);

SamplerState linear_wrap_sampler : register(s0, space0);

ConstantBuffer<PrimitiveConstantBuffer> primitive_cb : register(b0, space1);
StructuredBuffer<Vertex> vertex_buffer : register(t0, space1);
ByteAddressBuffer index_buffer : register(t1, space1);
Texture2D albedo_tex : register(t2, space1);
Texture2D metallic_glossiness_tex : register(t3, space1);
Texture2D emissive_tex : register(t4, space1);
Texture2D normal_tex : register(t5, space1);
Texture2D occlusion_tex : register(t6, space1);

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

float3 DiffuseColor(float3 albedo, float metallic)
{
    return albedo * (1 - metallic);
}

float3 SpecularColor(float3 albedo, float metallic)
{
    return lerp(0.04, albedo, metallic);
}

float3 FresnelTerm(float3 light_vec, float3 halfway_vec, float3 c_spec)
{
    float e_n = saturate(dot(light_vec, halfway_vec));
    return c_spec > 0 ? c_spec + (1 - c_spec) * exp2(-(5.55473f * e_n + 6.98316f) * e_n) : 0;
}

float SpecularNormalizeFactor(float glossiness)
{
    return (glossiness + 2) / 8;
}

float3 DistributionTerm(float3 halfway_vec, float3 normal, float glossiness)
{
    return exp((glossiness + 0.775f) * (max(dot(halfway_vec, normal), 0.0f) - 1));
}

float3 SpecularTerm(float3 c_spec, float3 light_vec, float3 halfway_vec, float3 normal, float glossiness)
{
    return SpecularNormalizeFactor(glossiness) * DistributionTerm(halfway_vec, normal, glossiness) *
           FresnelTerm(light_vec, halfway_vec, c_spec);
}

float AttenuationTerm(float3 light_pos, float3 pos, float3 atten)
{
    float3 v = light_pos - pos;
    float d2 = dot(v, v);
    float d = sqrt(d2);
    return 1 / dot(atten, float3(1, d, d2));
}

float4 CalcLighting(float3 position, float3 normal, float2 tex_coord)
{
    PbrMaterial mtl = material_buffer[primitive_cb.material_id];

    float3 const ambient = 0.1f;

    float3 const light_dir = normalize(scene_cb.light_pos.xyz - position);
    float3 const halfway = normalize(light_dir + normal);
    float const n_dot_l = max(0.0f, dot(light_dir, normal));

    float3 albedo = mtl.albedo * albedo_tex.SampleLevel(linear_wrap_sampler, tex_coord, 0).xyz;
    float opacity = mtl.opacity * albedo_tex.SampleLevel(linear_wrap_sampler, tex_coord, 0).w;
    float metallic = mtl.metallic * metallic_glossiness_tex.SampleLevel(linear_wrap_sampler, tex_coord, 0).y;

    float const MAX_GLOSSINESS = 8192;
    float glossiness = mtl.glossiness * pow(MAX_GLOSSINESS, metallic_glossiness_tex.SampleLevel(linear_wrap_sampler, tex_coord, 0).z);

    float3 c_diff = DiffuseColor(albedo, metallic);
    float3 c_spec = SpecularColor(albedo, metallic);

    float3 shading = AttenuationTerm(scene_cb.light_pos.xyz, position, scene_cb.light_falloff.xyz) * scene_cb.light_color.rgb *
                         max((c_diff + SpecularTerm(c_spec, light_dir, halfway, normal, glossiness)) * n_dot_l, 0) +
                     emissive_tex.SampleLevel(linear_wrap_sampler, tex_coord, 0).xyz;

    return float4(albedo * ambient + shading, opacity);
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

    float2 const vertex_tex_coords[] = {
        vertex_buffer[indices[0]].tex_coord, vertex_buffer[indices[1]].tex_coord, vertex_buffer[indices[2]].tex_coord};
    float2 const tex_coord = vertex_tex_coords[0] + attr.barycentrics.x * (vertex_tex_coords[1] - vertex_tex_coords[0]) +
                             attr.barycentrics.y * (vertex_tex_coords[2] - vertex_tex_coords[0]);

    payload.color = CalcLighting(hit_position, normal, tex_coord);
}

[shader("miss")]
void MissShader(inout RayPayload payload)
{
    float4 const background = float4(0.2f, 0.4f, 0.6f, 1.0f);
    payload.color = background;
}
