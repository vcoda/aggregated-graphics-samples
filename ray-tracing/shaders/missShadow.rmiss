#version 460
#extension GL_NV_ray_tracing : require

layout(location = 1) rayPayloadInNV bool shadow;

void main()
{
    shadow = false;
}
