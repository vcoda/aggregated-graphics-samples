#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "sobel.h"

layout(constant_id = 0) const bool c_showNormals = false;

layout(binding = 2) uniform DirectionalLight {
    vec3 viewDir;
} light;

layout(binding = 3) uniform sampler2D heightMap;

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec3 oColor;

void main()
{
    // compute normal from height map (2 texture gathers)
    const float strength = 10.;
    vec3 normal = sobel(heightMap, texCoord, strength);

    // transform from object space to view space
    vec3 n = mat3(normalMatrix) * normal.xzy;
    vec3 l = light.viewDir;
    float NdL = dot(n, l);

    if (c_showNormals)
        oColor = normal * .5 + .5;
    else
        oColor = vec3(max(NdL, 0.));
}
