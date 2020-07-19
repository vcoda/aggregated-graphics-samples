#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/linearizeDepth.h"

layout(binding = 0) uniform sampler2D gbufferLayer;

layout(location = 0) in vec2 texCoord;
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

// https://aras-p.info/texts/CompactNormalStorage.html
void decodeNormal()
{
    vec2 normal = texture(gbufferLayer, texCoord).rg;
    oColor = normalize(decode(normal));
}

void loadColor()
{
    oColor = texture(gbufferLayer, texCoord).rgb;
}

void loadShininess()
{
    float shininess = texture(gbufferLayer, texCoord).a;
    oColor = vec3(shininess);
}

void linearizeZ()
{
    float depth = texture(gbufferLayer, texCoord).r;
    float linearDepth = linearizeDepth(depth, 1., 100.);
    oColor = vec3(linearDepth);
}
