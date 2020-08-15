#version 450
#extension GL_GOOGLE_include_directive : enable
#include "shared.h"
#include "brdf/phong.h"

void main()
{
    vec3 n = normalize(viewNormal);
    vec3 l = normalize(light.viewDir.xyz);
    vec3 v = normalize(-viewPos);

    oColor = phong(n, l, v,
        surface.ambient.rgb, light.ambient.rgb,
        surface.diffuse.rgb, light.diffuse.rgb,
        surface.specular.rgb, light.specular.rgb,
        surface.shininess, 1.);

    sRgbFix();
}
