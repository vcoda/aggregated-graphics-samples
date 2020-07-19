#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/noise2d.h"
#include "pcf.h"

layout(binding = 2) uniform Light
{
    vec4 viewPos;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
} light;

layout(binding = 3) uniform Material
{
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
} surface;

layout(binding = 4) uniform Parameters
{
    vec4 screenSize; // x, y, 1/x, 1/y
    float radius;
    float zbias;
};

layout(binding = 5) uniform sampler2DShadow shadowMap;

layout(location = 0) in vec4 worldPos;
layout(location = 1) in vec3 viewPos;
layout(location = 2) in vec3 viewNormal;

layout(location = 0) out vec3 oColor;

float randf(vec4 fragCoord)
{
    vec2 uv = fragCoord.xy * screenSize.zw;
    float aspectRatio = screenSize.x * screenSize.w;
    uv.x *= aspectRatio;
    return noise(uv * screenSize.x);
}

mat2 jitter(float a, float x, float y)
{
    float s = sin(a);
    float c = cos(a);
    mat2 rot = mat2(
        c,-s,
        s, c);
    mat2 scale = mat2(
        x, 0.,
        0., y);
    return rot * scale;
}

vec3 phong(vec3 n, vec3 l, vec3 v,
           vec3 Ka, vec3 Ia,
           vec3 Kdiff, vec3 Idiff,
           vec3 Kspec, vec3 Ispec,
           float shininess,
           float shadow)
{
    float NdL = max(dot(n, l), 0.);
    vec3 r = reflect(-l, n);
    float RdV = max(dot(r, v), 0.);
    return Ka * Ia + shadow * (Kdiff * NdL * Idiff) + (Kspec * pow(RdV, shininess) * Ispec);
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
        float theta = randf(gl_FragCoord);
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
