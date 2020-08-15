// https://download.nvidia.com/developer/SDK/Individual_Samples/DEMOS/Direct3D9/src/HLSL_Aniso/docs/HLSL_Aniso.pdf
#version 450
#extension GL_GOOGLE_include_directive : enable
#include "shared.h"
#include "brdf/blinnPhong.h"

layout(binding = 4) uniform sampler2D aniso;

void main()
{
    vec3 n = normalize(viewNormal);
    vec3 l = normalize(light.viewDir.xyz);
    vec3 v = normalize(-viewPos);
    vec3 h = normalize(l + v);

    vec2 uv;
    uv.x = dot(n, l);
    uv.y = dot(n, h);
    vec4 tex = texture(aniso, uv);

    oColor = linear(tex.rgb * tex.aaa * 4.);
}
