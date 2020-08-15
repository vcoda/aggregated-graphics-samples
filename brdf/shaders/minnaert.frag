#version 450
#extension GL_GOOGLE_include_directive : enable
#include "shared.h"
#include "brdf/minnaert.h"

void main()
{
    vec3 n = normalize(viewNormal);
    vec3 l = normalize(light.viewDir.xyz);
    vec3 v = normalize(-viewPos);
    float k = 2.;

    oColor = minnaert(n, l, v,
        surface.diffuse.rgb, light.diffuse.rgb,
        k);

    sRgbFix();
}
