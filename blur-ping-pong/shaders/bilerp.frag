#version 450

layout(binding = 0) uniform sampler2D img;

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec3 oColor;

void main()
{
    oColor = textureLod(img, texCoord, 0).rgb;
}
