#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"

layout(location = 0) in vec4 position;

out gl_PerVertex{
    vec4 gl_Position;
};

void main()
{
    gl_Position = worldViewProj * position;
}
