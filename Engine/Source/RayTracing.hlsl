static uint const MaxRayRecursionDepth = 3;

// Ray types traced in this sample.
namespace RayType
{
    enum Enum
    {
        Radiance = 0, // ~ Primary, reflected camera/view rays calculating color for each hit.
        Shadow,       // ~ Shadow/visibility rays, only testing for occlusion
        Count
    };
}

namespace TraceRayParameters
{
    static uint const InstanceMask = ~0; // Everything is visible.

    namespace HitGroup
    {
        static uint const Offset[RayType::Count] = {
            0, // Radiance ray
            1  // Shadow ray
        };
        static const uint GeometryStride = RayType::Count;
    } // namespace HitGroup

    namespace MissShader
    {
        static uint const Offset[RayType::Count] = {
            0, // Radiance ray
            1  // Shadow ray
        };
    }
} // namespace TraceRayParameters

struct SceneConstantBuffer
{
    float4x4 inv_view_proj;
    float4 camera_pos;
};

struct Light
{
    float3 position;
    float3 color;
    float3 falloff;
    bool shadowing;
    uint4 paddings0;
    uint2 paddings1;
};

struct PbrMaterial
{
    float3 albedo;
    float opacity;
    float3 emissive;
    float metallic;
    float glossiness;
    float alpha_cutoff;
    float normal_scale;
    float occlusion_strength;
    bool transparent;
    bool two_sided;
    uint2 paddings;
};

struct Vertex
{
    float3 position;
    float4 tangent_quat;
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
StructuredBuffer<Light> light_buffer : register(t2, space0);

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
    return exp((glossiness + 0.775f) * (max(dot(halfway_vec, normal), 0) - 1));
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

struct RadianceRayPayload
{
    float4 color;
    uint recursion_depth;
};

struct ShadowRayPayload
{
    bool hit;
};

struct Ray
{
    float3 origin;
    float3 direction;
};

float4 TraceRadianceRay(Ray ray, uint curr_ray_recursion_depth)
{
    if (curr_ray_recursion_depth >= MaxRayRecursionDepth)
    {
        return float4(0, 0, 0, 0);
    }

    RayDesc ray_desc;
    ray_desc.Origin = ray.origin;
    ray_desc.Direction = ray.direction;
    ray_desc.TMin = 0.001f;
    ray_desc.TMax = 10000.0f;

    RadianceRayPayload payload = {float4(0, 0, 0, 0), curr_ray_recursion_depth + 1};
    TraceRay(scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, TraceRayParameters::InstanceMask,
        TraceRayParameters::HitGroup::Offset[RayType::Radiance], TraceRayParameters::HitGroup::GeometryStride,
        TraceRayParameters::MissShader::Offset[RayType::Radiance], ray_desc, payload);

    return payload.color;
}

bool TraceShadowRay(Ray ray, uint curr_ray_recursion_depth)
{
    if (curr_ray_recursion_depth >= MaxRayRecursionDepth)
    {
        return false;
    }

    RayDesc ray_desc;
    ray_desc.Origin = ray.origin;
    ray_desc.Direction = ray.direction;
    ray_desc.TMin = 0.03f;
    ray_desc.TMax = 10000.0f;

    ShadowRayPayload payload = {true};
    TraceRay(scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
        TraceRayParameters::InstanceMask, TraceRayParameters::HitGroup::Offset[RayType::Shadow],
        TraceRayParameters::HitGroup::GeometryStride, TraceRayParameters::MissShader::Offset[RayType::Shadow], ray_desc, payload);

    return payload.hit;
}

[shader("raygeneration")]
void RayGenShader()
{
    float2 pos_ss = (DispatchRaysIndex().xy + 0.5f) / DispatchRaysDimensions().xy * 2 - 1;
    pos_ss.y = -pos_ss.y;

    float4 pos_ws = mul(float4(pos_ss, 0, 1), scene_cb.inv_view_proj);
    pos_ws /= pos_ws.w;

    Ray ray;
    ray.origin = scene_cb.camera_pos.xyz;
    ray.direction = normalize(pos_ws.xyz - ray.origin);

    uint curr_recursion_depth = 0;
    float4 color = TraceRadianceRay(ray, curr_recursion_depth);

    render_target[DispatchRaysIndex().xy] = color;
}

float3 TransformQuat(float3 v, float4 quat)
{
    return v + cross(quat.xyz, cross(quat.xyz, v) + quat.w * v) * 2;
}

float4 CalcLighting(float3 position, float3x3 tangent_frame, float2 tex_coord, uint recursion_depth)
{
    PbrMaterial mtl = material_buffer[primitive_cb.material_id];

    float const ambient_factor = 0.1f;

    float4 const albedo_data = albedo_tex.SampleLevel(linear_wrap_sampler, tex_coord, 0);
    float3 const albedo = mtl.albedo * albedo_data.xyz;
    float const opacity = mtl.opacity * albedo_data.w;

    float const metallic = mtl.metallic * metallic_glossiness_tex.SampleLevel(linear_wrap_sampler, tex_coord, 0).y;

    float const MAX_GLOSSINESS = 8192;
    float const glossiness = mtl.glossiness * pow(MAX_GLOSSINESS, metallic_glossiness_tex.SampleLevel(linear_wrap_sampler, tex_coord, 0).z);

    float const occlusion = mtl.occlusion_strength * occlusion_tex.SampleLevel(linear_wrap_sampler, tex_coord, 0).x;

    float3 normal = normal_tex.SampleLevel(linear_wrap_sampler, tex_coord, 0).xyz * 2 - 1;
    normal = normalize(mul(normal * float3(mtl.normal_scale.xx, 1), tangent_frame));

    float3 shading = 0;

    uint num_lights;
    uint light_stride;
    light_buffer.GetDimensions(num_lights, light_stride);
    for (uint i = 0; i < num_lights; ++i)
    {
        Light light = light_buffer[i];

        float3 const light_dir = normalize(light.position - position);

        bool in_shadow = false;
        if (light.shadowing)
        {
            Ray const shadow_ray = {position, light_dir};
            in_shadow = TraceShadowRay(shadow_ray, recursion_depth);
        }

        if (!in_shadow)
        {
            float3 const halfway = normalize(light_dir + normal);
            float const n_dot_l = max(dot(light_dir, normal), 0);

            float const attenuation = AttenuationTerm(light.position, position, light.falloff);

            float3 const c_diff = DiffuseColor(albedo, metallic);
            float3 const c_spec = SpecularColor(albedo, metallic);

            float3 const diffuse = c_diff;
            float3 const specular = SpecularTerm(c_spec, light_dir, halfway, normal, glossiness);

            shading += attenuation * occlusion * max((diffuse + specular) * n_dot_l, 0) * light.color;
        }
    }

    float3 const ambient = ambient_factor * albedo;
    float3 const emissive = emissive_tex.SampleLevel(linear_wrap_sampler, tex_coord, 0).xyz;

    float3 color = ambient + emissive + shading;
    if (mtl.transparent && (opacity < 1.0f - 0.5f / 255.0f))
    {
        Ray const ray = {position, WorldRayDirection()};
        color = lerp(TraceRadianceRay(ray, recursion_depth).xyz, color, opacity);
    }

    return float4(color, 1);
}

[shader("anyhit")]
void AnyHitShader(inout RadianceRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint const index_size = 2;
    uint const indices_per_triangle = 3;
    uint const triangle_index_stride = indices_per_triangle * index_size;
    uint const base_index = PrimitiveIndex() * triangle_index_stride;

    uint3 const indices = Load3x16BitIndices(base_index);

    float2 const vertex_tex_coords[] = {
        vertex_buffer[indices[0]].tex_coord, vertex_buffer[indices[1]].tex_coord, vertex_buffer[indices[2]].tex_coord};
    float2 const tex_coord = vertex_tex_coords[0] + attr.barycentrics.x * (vertex_tex_coords[1] - vertex_tex_coords[0]) +
                             attr.barycentrics.y * (vertex_tex_coords[2] - vertex_tex_coords[0]);

    PbrMaterial mtl = material_buffer[primitive_cb.material_id];

    float4 const albedo_data = albedo_tex.SampleLevel(linear_wrap_sampler, tex_coord, 0);
    float const opacity = mtl.opacity * albedo_data.w;

    if (opacity < mtl.alpha_cutoff)
    {
        IgnoreHit();
    }
}

[shader("closesthit")]
void ClosestHitShader(inout RadianceRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    float3 const hit_position = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

    uint const index_size = 2;
    uint const indices_per_triangle = 3;
    uint const triangle_index_stride = indices_per_triangle * index_size;
    uint const base_index = PrimitiveIndex() * triangle_index_stride;

    uint3 const indices = Load3x16BitIndices(base_index);

    float3 const vertex_tangents[] = {
        TransformQuat(float3(1, 0, 0), vertex_buffer[indices[0]].tangent_quat),
        TransformQuat(float3(1, 0, 0), vertex_buffer[indices[1]].tangent_quat),
        TransformQuat(float3(1, 0, 0), vertex_buffer[indices[2]].tangent_quat),
    };
    float3 const vertex_bitangents[] = {
        TransformQuat(float3(0, 1, 0), vertex_buffer[indices[0]].tangent_quat),
        TransformQuat(float3(0, 1, 0), vertex_buffer[indices[1]].tangent_quat),
        TransformQuat(float3(0, 1, 0), vertex_buffer[indices[2]].tangent_quat),
    };
    float3 const vertex_normals[] = {
        TransformQuat(float3(0, 0, 1), vertex_buffer[indices[0]].tangent_quat),
        TransformQuat(float3(0, 0, 1), vertex_buffer[indices[1]].tangent_quat),
        TransformQuat(float3(0, 0, 1), vertex_buffer[indices[2]].tangent_quat),
    };

    float3 const tangent = vertex_tangents[0] + attr.barycentrics.x * (vertex_tangents[1] - vertex_tangents[0]) +
                           attr.barycentrics.y * (vertex_tangents[2] - vertex_tangents[0]);
    float3 const bitangent = vertex_bitangents[0] + attr.barycentrics.x * (vertex_bitangents[1] - vertex_bitangents[0]) +
                             attr.barycentrics.y * (vertex_bitangents[2] - vertex_bitangents[0]);
    float3 const normal = vertex_normals[0] + attr.barycentrics.x * (vertex_normals[1] - vertex_normals[0]) +
                          attr.barycentrics.y * (vertex_normals[2] - vertex_normals[0]);

    float4x3 const model_matrix_4x3 = ObjectToWorld4x3();
    float4x4 const model_matrix = {
        float4(model_matrix_4x3[0], 0),
        float4(model_matrix_4x3[1], 0),
        float4(model_matrix_4x3[2], 0),
        float4(model_matrix_4x3[3], 1),
    };
    float4x3 const inv_model_matrix_4x3 = WorldToObject4x3();
    float4x4 const inv_model_matrix = {
        float4(inv_model_matrix_4x3[0], 0),
        float4(inv_model_matrix_4x3[1], 0),
        float4(inv_model_matrix_4x3[2], 0),
        float4(inv_model_matrix_4x3[3], 1),
    };
    float4x4 const model_matrix_it = transpose(inv_model_matrix);
    float3x3 const tangent_frame = {
        normalize(mul(normalize(tangent), (float3x3)model_matrix)),
        normalize(mul(normalize(bitangent), (float3x3)model_matrix)),
        normalize(mul(normalize(normal), (float3x3)model_matrix_it)),
    };

    float2 const vertex_tex_coords[] = {
        vertex_buffer[indices[0]].tex_coord, vertex_buffer[indices[1]].tex_coord, vertex_buffer[indices[2]].tex_coord};
    float2 const tex_coord = vertex_tex_coords[0] + attr.barycentrics.x * (vertex_tex_coords[1] - vertex_tex_coords[0]) +
                             attr.barycentrics.y * (vertex_tex_coords[2] - vertex_tex_coords[0]);

    payload.color = CalcLighting(hit_position, tangent_frame, tex_coord, payload.recursion_depth);
}

[shader("miss")]
void MissShaderRadianceRay(inout RadianceRayPayload payload)
{
    float4 const background = float4(0.2f, 0.4f, 0.6f, 1.0f);
    payload.color = background;
}

[shader("miss")]
void MissShaderShadowRay(inout ShadowRayPayload payload)
{
    payload.hit = false;
}
