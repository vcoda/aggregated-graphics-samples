#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/reconstruct.h"
#include "common/cotangentFrame.h"
#include "common/sRGB.h"

layout(constant_id = 0) const bool c_showNormals = false;

layout(binding = 2) uniform Light {
    vec3 viewPos;
} light;

layout(binding = 3) uniform sampler2D normalMap;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 viewPos;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texCoord;

layout(location = 0) out vec3 oColor;

void main()
{
    vec2 nxy = texture(normalMap, texCoord).xy;

    // compute per-pixel cotangent frame
    vec3 N = normalize(normal);
    mat3 TBN = cotangentFrame(N, position, texCoord);

    vec3 l = normalize(light.viewPos - viewPos);

    // transform from texture space to object space
    vec3 micronormal = TBN * reconstructNormal(nxy);
    // transform from object space to view space
    vec3 n = mat3(normalMatrix) * micronormal;
    n = normalize(n);

    float NdL = dot(n, l);

    if (c_showNormals)
        oColor = linear(micronormal * .5 + .5);
    else
        oColor = vec3(max(NdL, 0.));
}
