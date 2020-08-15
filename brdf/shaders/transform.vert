#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"

layout(location = 0) in vec4 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 oPos;
layout(location = 1) out vec3 oViewPos;
layout(location = 2) out vec3 oNormal;
layout(location = 3) out vec3 oViewNormal;
layout(location = 4) out vec2 oTexCoord;
out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    oPos = position.xyz;
    oViewPos = (worldView * position).xyz;
    oNormal = normal;
    oViewNormal = mat3(normalMatrix) * normal;
    oTexCoord = texCoord;
    gl_Position = worldViewProj * position;
}
