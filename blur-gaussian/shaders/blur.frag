#version 450

#define N 15 // odd

layout(binding = 0) uniform sampler2D img;
layout(binding = 1) buffer Weights {
    float weights[N + 1]; // + norm factor
};

layout(location = 0) in vec4 texCoord; // u, v, du, dv
layout(location = 0) out vec3 oColor;

void main()
{
    vec2 uv = texCoord.xy;
    vec2 duv = texCoord.zw;

    // Gaussian filter
    oColor = vec3(0.);
    for (int i = 0; i < N; ++i, uv += duv)
        oColor += textureLod(img, uv, 0).rgb * weights[i];
    oColor.rgb *= weights[N]; // normalize
}
