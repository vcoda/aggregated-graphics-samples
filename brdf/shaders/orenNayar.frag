#version 450
#extension GL_GOOGLE_include_directive : enable
#include "shared.h"
#include "brdf/orenNayar.h"

void main()
{
    vec3 n = normalize(viewNormal);
    vec3 l = normalize(light.viewDir.xyz);
    vec3 v = normalize(-viewPos);
    float roughness = 0.8;

    oColor = orenNayar(n, l, v,
        surface.ambient.rgb, light.ambient.rgb,
        surface.diffuse.rgb, light.diffuse.rgb,
        roughness, 1.);

    sRgbFix();
}
