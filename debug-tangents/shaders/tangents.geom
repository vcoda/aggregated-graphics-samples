#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"

layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

layout(location = 0) in vec3 pos[];
layout(location = 1) in vec3 normals[];
layout(location = 2) in vec2 texCoords[];

layout(location = 0) out vec3 oColor;

void emitLine(vec3 v, vec3 color)
{
    oColor = color;
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    oColor = color;
    gl_Position = worldViewProj * vec4(pos[0] + v * 0.05, 1.);
    EmitVertex();
    EndPrimitive();
}

void main(void)
{
    vec3 v1 = pos[1] - pos[0];
    vec3 v2 = pos[2] - pos[0];
    vec2 st1 = texCoords[1].xy - texCoords[0].xy;
    vec2 st2 = texCoords[2].xy - texCoords[0].xy;
    vec3 normal = normalize(normals[0]);

    // http://www.mathfor3dgameprogramming.com/code/Listing7.1.cpp
    float r = 1./(st1.x * st2.y - st2.x * st1.y);
    vec3 t = (v1 * st2.y - v2 * st1.y) * r; // S dir
    vec3 b = (v2 * st1.x - v1 * st2.x) * r; // T dir
    vec3 n = normal;

    // Gram-Schmidt orthogonalize
    vec3 tangent = normalize(t - n * dot(n, t));
    // calculate handedness
    float handedness = (dot(cross(n, t), b) < 0.) ? -1. : 1.;
    // http://www.mathfor3dgameprogramming.com/code/Listing7.2.vs
    vec3 bitangent = cross(normal, tangent) * handedness;

    emitLine(tangent, vec3(1, 0, 0));
    emitLine(bitangent, vec3(0, 1, 0));
    emitLine(normal, vec3(0, 0.5, 1));
}
