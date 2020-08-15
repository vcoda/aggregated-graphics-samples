#version 450
#extension GL_GOOGLE_include_directive : enable
#include "shared.h"

void main()
{
    vec3 n = normalize(viewNormal);
    vec3 l = normalize(light.viewDir.xyz);

    oColor = max(dot(n, l), 0.) * surface.diffuse.rgb * light.diffuse.rgb;

    sRgbFix();
}
