#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) out vec3 barycentric;

void main(void)
{
    const vec3 barycentrics[gl_in.length()] = vec3[](
        vec3(1, 0, 0),
        vec3(0, 1, 0),
        vec3(0, 0, 1)
    );
    for (int i = 0; i < gl_in.length(); ++i)
    {
        barycentric = barycentrics[i];
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
}
