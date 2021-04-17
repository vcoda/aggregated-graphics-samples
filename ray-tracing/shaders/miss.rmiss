#version 460
#extension GL_NV_ray_tracing : require

layout(location = 0) rayPayloadInNV vec3 color;

void main()
{
    color = vec3(1, 0, 0);
}
