#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/reconstruct.h"
#include "common/brdf/phong.h"

struct Gbuffer
{
    vec2 normal;
    vec3 albedo;
    float ambient;
    vec3 specular;
    float shininess;
    float depth;
};

layout(binding = 2) uniform Light
{
    vec4 viewPos;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
} light;

layout(binding = 3) uniform sampler2D gbufferNormal;
layout(binding = 4) uniform sampler2D gbufferAlbedo;
layout(binding = 5) uniform sampler2D gbufferSpecular;
layout(binding = 6) uniform sampler2D gbufferDepth;

layout(location = 0) in vec2 screenPos;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec3 oColor;

// https://aras-p.info/texts/CompactNormalStorage.html
vec3 decode(vec2 enc)
{
    vec4 nn = vec4(enc * 2., 0., 0.) + vec4(-1., -1., 1., -1.);
    float l = dot(nn.xyz, -nn.xyw);
    nn.z = l;
    nn.xy *= sqrt(l);
    return nn.xyz * 2. + vec3(0., 0., -1.);
}

Gbuffer loadGbuffer()
{
    Gbuffer gbuffer;
    gbuffer.normal = texture(gbufferNormal, texCoord).rg;
    vec4 albedo = texture(gbufferAlbedo, texCoord);
    vec4 specular = texture(gbufferSpecular, texCoord);
    gbuffer.albedo = albedo.rgb;
    gbuffer.ambient = albedo.a;
    gbuffer.specular = specular.rgb;
    gbuffer.shininess = specular.a * 256.;
    gbuffer.depth = texture(gbufferDepth, texCoord).r;
    return gbuffer;
}

void main()
{
    Gbuffer gbuffer = loadGbuffer();
    if (1. == gbuffer.depth)
        discard;

    vec3 viewPos = reconstructViewPos(screenPos, gbuffer.depth);
    vec3 n = normalize(decode(gbuffer.normal));
    vec3 l = normalize(light.viewPos.xyz - viewPos);
    vec3 v = normalize(-viewPos); // view position at (0, 0, 0)

    oColor = phong(n, l, v,
        gbuffer.albedo.rgb * gbuffer.ambient, light.ambient.rgb,
        gbuffer.albedo.rgb, light.diffuse.rgb,
        gbuffer.specular.rgb, light.specular.rgb,
        gbuffer.shininess, 1.);
}
