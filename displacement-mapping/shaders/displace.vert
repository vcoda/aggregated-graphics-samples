#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"

layout(binding = 4) uniform Parameters {
    vec4 screenSize;
    float displacement;
};

layout(binding = 5) uniform sampler2D displacementMap;

layout(location = 0) in vec4 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec4 oPos;
layout(location = 1) out vec3 oViewPos;
layout(location = 2) out vec3 oNormal;
layout(location = 3) out vec2 oTexCoord;

out gl_PerVertex{
    vec4 gl_Position;
};

void main()
{
    float bump = textureLod(displacementMap, texCoord, 0).w;
    vec3 displace = normal * bump * displacement;
    oPos = vec4(position.xyz + displace, 1.);
    oViewPos = (worldView * oPos).xyz;
    oNormal = normal;
    oTexCoord = texCoord;
    gl_Position = worldViewProj * oPos;
}
