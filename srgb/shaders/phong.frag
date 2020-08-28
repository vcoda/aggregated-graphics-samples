#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/sRGB.h"
#include "brdf/phong.h"

layout(binding = 2) uniform Light {
    vec3 viewDir;
} light;

layout(constant_id = 0) const bool c_sRGB = false;

layout(location = 0) in vec3 viewPos;
layout(location = 1) in vec3 viewNormal;

layout(location = 0) out vec3 oColor;

vec3 gamma(vec3 color)
{
    return c_sRGB ? linear(color) : color;
}

void main()
{
    const float ambient = 0.2;
    const vec3 alice_blue = vec3(240, 248, 255)/255.;
    const vec3 white = vec3(1.);
    const float shininess = 128.;

    vec3 n = normalize(viewNormal);
    vec3 l = normalize(light.viewDir);
    vec3 v = normalize(-viewPos);

    oColor = phong(n, l, v,
        gamma(alice_blue * ambient), gamma(white * ambient),
        gamma(alice_blue), gamma(white),
        gamma(alice_blue), gamma(white),
        shininess, 1.);

    if (c_sRGB)
        oColor = sRGB(oColor);
}
