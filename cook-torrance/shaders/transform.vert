#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"

layout(location = 0) in vec4 position;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec3 oViewPos;
layout(location = 1) out vec3 oViewNormal;
out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    oViewPos = (worldView * position).xyz;
    oViewNormal = mat3(normalMatrix) * normal;
    gl_Position = worldViewProj * position;
}
