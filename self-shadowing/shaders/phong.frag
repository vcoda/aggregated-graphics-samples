#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/reconstruct.h"
#include "common/cotangentFrame.h"
#include "common/brdf/phong.h"
#include "common/sRGB.h"

layout(constant_id = 0) const bool c_selfShadowing = false;

layout(binding = 2) uniform Light
{
    vec4 viewPos;
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
} light;

layout(binding = 3) uniform sampler2D diffuseMap;
layout(binding = 4) uniform sampler2D normalMap;
layout(binding = 5) uniform sampler2D specularMap;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 viewPos;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 viewNormal;
layout(location = 4) in vec2 texCoord;

layout(location = 0) out vec3 oColor;

float selfshadowing(vec3 n, vec3 l, float c)
{
    float NdL = dot(n, l);
    return (NdL > c) ? 1. :
        (NdL <= 0.) ? 0. :
        1./c * NdL;
}

void main()
{
    vec2 nxy = texture(normalMap, texCoord).xy;
    vec3 albedo = texture(diffuseMap, texCoord).rgb;
    float shininess = texture(specularMap, texCoord).x;

    // compute per-pixel cotangent frame
    vec3 N = normalize(normal);
    mat3 TBN = cotangentFrame(N, position, texCoord);

    // transform from texture space to object space
    vec3 micronormal = TBN * reconstructNormal(nxy);
    // transform from object space to view space
    micronormal = mat3(normalMatrix) * micronormal;

    vec3 n = normalize(micronormal);
    vec3 l = normalize(light.viewPos.xyz - viewPos);
    vec3 v = -normalize(viewPos);

    float self = 1.;
    if (c_selfShadowing)
    {   // compute self-shadowing factor
        vec3 n = normalize(viewNormal);
        self = selfshadowing(n, l, 1./8.);
    }

    vec3 srgbAlbedo = sRGB(albedo);
    vec3 ambient = linear(0.2 * srgbAlbedo);
    vec3 specular = linear(2. * srgbAlbedo);

    oColor = phong(n, l, v,
        ambient, light.ambient.rgb,
        albedo, light.diffuse.rgb,
        specular, light.specular.rgb,
        shininess * 16., self);
}
