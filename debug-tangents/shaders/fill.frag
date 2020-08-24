#version 450

layout(location = 0) in vec3 color;
layout(location = 0) out vec3 oColor;

void main()
{
    oColor = color;
}
