#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/sRGB.h"
#define D_BECKMANN
#define G_COOK_TORRANCE
#define RD_ASHIKHMIN_SHIRLEY
#include "brdf/cookTorrance.h"

layout(constant_id = 0) const bool c_gammaCorrection = true;

struct DirectionalLight
{
    vec3 viewDir;
    vec3 diffuse;
};

layout(binding = 2) buffer Lights {
    DirectionalLight lights[2];
};

layout(binding = 3) uniform Roughness {
    float roughness;
};

layout(binding = 4) uniform RefractiveIndices {
    vec3 rgb;
} f0;

layout(binding = 5) uniform BaseColors {
    vec3 albedo;
};

layout(location = 0) in vec3 viewPos;
layout(location = 1) in vec3 viewNormal;

layout(location = 0) out vec3 oColor;

void main()
{
    vec3 n = normalize(viewNormal);
    vec3 l0 = lights[0].viewDir;
    vec3 l1 = lights[1].viewDir;
    vec3 v = normalize(-viewPos);

    oColor = cookTorrance(n, l0, v, f0.rgb, roughness, albedo, lights[0].diffuse);
    oColor += cookTorrance(n, l1, v, f0.rgb, roughness, albedo, lights[1].diffuse);

    if (!c_gammaCorrection)
        oColor = linear(oColor); // revert x^1/2.2 in the framebuffer
}
