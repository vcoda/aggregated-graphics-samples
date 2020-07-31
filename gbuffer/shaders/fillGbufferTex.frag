#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/cotangentFrame.h"

layout(binding = 2) uniform Material {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
} surface;

layout(binding = 3) uniform sampler2D normalMap;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

// G-buffer color targets
layout(location = 0) out vec2 oViewNormal;
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
    vec3 micronormal = texture(normalMap, texCoord).rgb;

    // compute per-pixel cotangent frame
    vec3 N = normalize(normal);
    mat3 TBN = cotangentFrame(N, position, texCoord);

    // transform from texture space to object space
    micronormal = TBN * normalize(micronormal * 2. - 1.);
    // transform from object space to view space
    vec3 viewNormal = mat3(normalMatrix) * micronormal;

    oViewNormal = encode(viewNormal);
    oAlbedo = surface.diffuse;
    oSpecular = vec4(surface.specular.rgb, surface.shininess/256.);
}
