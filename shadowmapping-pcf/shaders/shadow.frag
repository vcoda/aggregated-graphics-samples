#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/sRGB.h"
#include "pcf.h"
#include "pcfPoisson.h"

layout(constant_id = 0) const bool c_filterPoisson = false;

layout(binding = 2) uniform LightSource {
    vec4 viewPos;
} light;

layout(binding = 3) uniform sampler2DShadow shadowMap;

layout(location = 0) in vec4 worldPos;
layout(location = 1) in vec3 viewPos;
layout(location = 2) in vec3 viewNormal;

layout(location = 0) out vec3 oColor;

void main()
{
    vec3 n = normalize(viewNormal);
    vec3 l = normalize(light.viewPos.xyz - viewPos);
    float NdL = dot(n, l);

    float shadow;
    if (NdL <= 0.)
        shadow = 0.;
    else
    {
        vec4 clipPos = shadowProj * worldPos;
        if (c_filterPoisson)
            shadow = pcfPoisson(shadowMap, clipPos);
        else
            shadow = pcf(shadowMap, clipPos);
    }

    const vec3 floral_white = linear(vec3(1., 0.98, 0.941));
    const float ambient = linear(0.16);

    oColor = ambient + max(NdL, 0.) * floral_white * shadow;
}
