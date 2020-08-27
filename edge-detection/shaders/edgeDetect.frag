#version 450
#extension GL_GOOGLE_include_directive : enable
#include "sobel.h"

layout(constant_id = 0) const bool c_sobelFilter = true;

layout(binding = 0) uniform sampler2D depthMap;

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec3 oColor;

void main()
{
    float intensity;
    if (c_sobelFilter)
    {
        intensity = sobel(depthMap, texCoord);
    }
    else
    {
        vec4 z = textureGather(depthMap, texCoord);
        vec4 w = fwidth(z);
        intensity = dot(w, vec4(1)); // sum
    }
    oColor = vec3(step(0.002, intensity));
}
