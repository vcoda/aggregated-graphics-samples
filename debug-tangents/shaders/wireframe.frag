// https://web.archive.org/web/20120915031732/http://codeflow.org/entries/2012/aug/02/easy-wireframe-display-with-barycentric-coordinates/
#version 450

layout(binding = 1) uniform Wireframe {
    vec3 color;
    float lineWidth;
};

layout(location = 0) in vec3 barycentric;
layout(location = 0) out vec4 oColor;

void main()
{
    vec3 d = fwidth(barycentric);
    vec3 t = smoothstep(vec3(0), d * lineWidth, barycentric);
    float a = 1. - min(min(t.x, t.y), t.z);
    oColor = vec4(color, a);
}
