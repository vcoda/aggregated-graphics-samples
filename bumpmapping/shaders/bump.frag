#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/cotangentFrame.h"
#include "common/sRGB.h"
#include "sobel.h"

layout(constant_id = 0) const bool c_showHeightmap = false;

layout(binding = 2) uniform Light
{
    vec3 viewPos;
} light;

layout(binding = 3) uniform sampler2D heightMap;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 viewPos;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texCoord;

layout(location = 0) out vec3 oColor;

void main()
{
    // compute normal from height map (8 texture samples)
    const float strength = 6.;
    vec3 mapNormal = sobel(heightMap, texCoord, strength);

    // compute per-pixel cotangent frame
    vec3 N = normalize(normal);
    mat3 TBN = cotangentFrame(N, position, texCoord);

    // transform from texture space to object space
    vec3 normal = TBN * mapNormal;
    // transform from object space to view space
    vec3 viewNormal = mat3(normalMatrix) * normal;

    vec3 n = normalize(viewNormal);
    vec3 l = normalize(light.viewPos - viewPos);
    float NdL = dot(n, l);

    if (c_showHeightmap)
        oColor = vec3(linear(texture(heightMap, texCoord).x));
    else
        oColor = vec3(max(NdL, 0.));
}
