#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/sRGB.h"

layout(binding = 2) uniform LightSource
{
    vec4 viewPos;
} light;

layout(binding = 3) uniform sampler2DShadow shadowMap;

layout(location = 0) in vec4 worldPos;
layout(location = 1) in vec3 viewPos;
layout(location = 2) in vec3 viewNormal;

layout(location = 0) out vec3 oColor;

void main()
{
    vec4 clipPos = shadowProj * worldPos;
    float shadow = textureProj(shadowMap, clipPos);

    vec3 n = normalize(viewNormal);
    vec3 l = normalize(light.viewPos.xyz - viewPos);
    float NdL = dot(n, l);
    vec3 floral_white = linear(vec3(1.0, 0.98, 0.941));
    float ambient = linear(0.16);

    oColor = ambient + max(NdL, 0.) * floral_white * shadow;
}
