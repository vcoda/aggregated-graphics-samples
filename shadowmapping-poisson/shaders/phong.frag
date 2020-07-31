#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/noise2d.h"
#include "common/jitter.h"
#include "common/brdf/phong.h"
#include "pcf.h"

layout(binding = 2) uniform Light {
    vec4 viewPos;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
} light;

layout(binding = 3) uniform Material {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
} surface;

layout(binding = 4) uniform Parameters {
    vec4 screenSize; // x, y, 1/x, 1/y
    float radius;
    float zbias;
};

layout(binding = 5) uniform sampler2DShadow shadowMap;

layout(location = 0) in vec4 worldPos;
layout(location = 1) in vec3 viewPos;
layout(location = 2) in vec3 viewNormal;

layout(location = 0) out vec3 oColor;

float screenNoise(vec4 fragCoord)
{
    vec2 uv = fragCoord.xy * screenSize.zw;
    float aspectRatio = screenSize.x * screenSize.w;
    uv.x *= aspectRatio;
    return noise(uv * screenSize.x);
}

void main()
{
    vec3 n = normalize(viewNormal);
    vec3 l = normalize(light.viewPos.xyz - viewPos);
    vec3 v = -normalize(viewPos);

    float shadow;
    if (dot(n, l) <= 0.)
        shadow = 0.;
    else
    {
        float theta = screenNoise(gl_FragCoord);
        vec4 shadowPos = shadowProj * worldPos;
        vec2 scale = radius/textureSize(shadowMap, 0) * shadowPos.w;
        mat2 jitMat = jitter(theta * TWO_PI, scale.x, scale.y);
        shadow = pcf(shadowMap, shadowPos, zbias, jitMat);
    }

    oColor = phong(n, l, v,
        surface.ambient.rgb, light.ambient.rgb,
        surface.diffuse.rgb, light.diffuse.rgb,
        surface.specular.rgb, light.specular.rgb,
        surface.shininess, shadow);
}
