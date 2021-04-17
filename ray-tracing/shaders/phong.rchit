#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#include "rt/interpolation.h"
#include "brdf/phong.h"
#include "common/sRGB.h"

#define MAX_OBJECTS 4

struct Vertex
{
    vec3 pos;       // +padding
    vec3 normal;    // +padding
    vec4 color;
};

struct Transforms
{
    mat4 world;
    mat4 normal;
};

hitAttributeNV vec2 attribs;

layout(location = 0) rayPayloadInNV vec3 oColor;
layout(location = 1) rayPayloadNV bool shadow;

layout(set = 0, binding = 0) uniform accelerationStructureNV tlas;

layout(set = 1, binding = 0) buffer Vertices {
    Vertex vertices[]; 
} vertexBuffers[];

layout(set = 1, binding = 1) buffer Indices {
    uint indices[]; 
} indexBuffers[];

layout(set = 1, binding = 2) uniform Light
{
    vec4 worldPos;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
} light;

layout(set = 1, binding = 3) uniform WorldTransforms {
     Transforms transforms[MAX_OBJECTS];
};

void main()
{
    // get triangle vertex indices
    uint offset = gl_PrimitiveID * 3;
    ivec3 ind = ivec3(indexBuffers[gl_InstanceID].indices[offset + 0],
                      indexBuffers[gl_InstanceID].indices[offset + 1],
                      indexBuffers[gl_InstanceID].indices[offset + 2]);

    // get triangle vertices
    Vertex v0 = vertexBuffers[gl_InstanceID].vertices[ind.x];
    Vertex v1 = vertexBuffers[gl_InstanceID].vertices[ind.y];
    Vertex v2 = vertexBuffers[gl_InstanceID].vertices[ind.z];

    // compute barycentric coordinates and interpolate vertex attributes
    vec3 bary = vec3(1. - attribs.x - attribs.y, attribs.xy);
    vec3 pos = interpolate(v0.pos, v1.pos, v2.pos, bary);
    vec3 normal = interpolate(v0.normal, v1.normal, v2.normal, bary);
    vec3 diffuse = interpolate(linear(v0.color.rgb), linear(v1.color.rgb), linear(v2.color.rgb), bary);

    vec3 Ka = diffuse * 0.2;
    vec3 Ia = light.ambient.rgb;
    oColor = Ka * Ia;

    // compute N.L
    vec3 worldPos = (transforms[gl_InstanceID].world * vec4(pos, 1.)).xyz;
    vec3 lightDir = light.worldPos.xyz - worldPos;
    vec3 n = normalize(mat3(transforms[gl_InstanceID].normal) * normal);
    vec3 l = normalize(lightDir);
    float NdL = dot(n, l);

    if (NdL > 0.)
    {
        vec3 rayOrigin = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
        uint rayFlags = gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV;
        uint cullMask = 0xFF;
        float tmin = 0.001;
        float tmax = length(lightDir);

        shadow = true;
        traceNV(tlas, rayFlags, cullMask, 0, 0,
            1, // miss index
            rayOrigin, tmin, l, tmax,
            1); // payload location

        if (!shadow)
        {
            vec3 v = normalize(gl_WorldRayOriginNV - worldPos);
            oColor = phong(n, l, v,
                Ka, Ia,
                diffuse, light.diffuse.rgb,
                diffuse, light.specular.rgb,
                4., 1.);
        }
    }
}
