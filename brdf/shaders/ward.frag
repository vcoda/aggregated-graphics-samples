#version 450
#extension GL_GOOGLE_include_directive : enable
#include "shared.h"
#include "common/cotangentFrame.h"
#include "brdf/ward.h"

layout(binding = 4) uniform sampler2D aniso;

void main()
{
    vec3 n = normalize(viewNormal);
    vec3 l = normalize(light.viewDir.xyz);
    vec3 v = normalize(-viewPos);
    mat3 TBN = cotangentFrame(normalize(normal), position, texCoord);
    vec3 t = mat3(normalMatrix) * TBN[0];
    vec3 b = mat3(normalMatrix) * TBN[1];

    vec2 uv = vec2(1, 1);
    vec3 diffuse = linear(texture(aniso, uv).rgb);
    float ax = 0.25;
    float ay = 0.25;

    oColor = ward(n, l, v, t, b,
        ax, ay,
        diffuse, light.diffuse.rgb,
        diffuse, light.specular.rgb);

    sRgbFix();
}
