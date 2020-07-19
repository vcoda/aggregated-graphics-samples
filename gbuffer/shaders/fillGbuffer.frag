#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"

layout(binding = 2) uniform Material
{
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
} surface;

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec2 texCoord;

// G-buffer color targets
layout(location = 0) out vec2 oNormal;
layout(location = 1) out vec4 oAlbedo;
layout(location = 2) out vec4 oSpecular;

// https://aras-p.info/texts/CompactNormalStorage.html
vec2 encode(vec3 n)
{
    vec2 enc = normalize(n.xy) * (sqrt(-n.z * .5 + .5));
    enc = enc * .5 + .5;
    return enc;
}

void main()
{
    vec3 viewNormal = mat3(view) * worldNormal;
    oNormal = encode(normalize(viewNormal));
    oAlbedo = surface.diffuse;
    oSpecular = vec4(surface.specular.rgb, surface.shininess/256.);
}
