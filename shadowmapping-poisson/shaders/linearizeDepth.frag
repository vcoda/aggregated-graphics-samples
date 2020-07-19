#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/linearizeDepth.h"

#define NEAR 5.
#define FAR 30.

layout(binding = 0) uniform sampler2D depth;

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec3 oColor;

void main()
{
    float z = texture(depth, texCoord).r;
    z = linearizeDepth(z, NEAR, FAR);
    oColor = vec3(z);
}
