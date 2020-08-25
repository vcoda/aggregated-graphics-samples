#version 450
#extension GL_GOOGLE_include_directive : enable
#include "sobel.h"

layout(binding = 0) uniform sampler2D depthMap;

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec3 oColor;

void main()
{
    float intensity = sobel(depthMap, texCoord);
    oColor = vec3(step(0.002, intensity));
}
