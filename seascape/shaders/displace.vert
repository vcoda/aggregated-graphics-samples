#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"

layout(binding = 5) uniform sampler2D heightMap;

layout(location = 0) in vec2 pos; // X, Z of grid mesh

layout(location = 0) out vec3 oViewPos;
layout(location = 1) out vec2 oTexCoord;
out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    const float gridScale = 32.;
    vec2 texCoord = pos.xy/gridScale + 0.5;
    // fetch value from height map
    float h = textureLod(heightMap, texCoord, 0).x;
    vec4 position = vec4(pos.x, h, pos.y, 1.);
    oViewPos = (worldView * position).xyz;
    oTexCoord = texCoord;
    gl_Position = worldViewProj * position;
}
