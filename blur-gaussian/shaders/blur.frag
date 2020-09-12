#version 450

#define N 15 // odd

layout(constant_id = 0) const bool c_horzPass = true;

layout(binding = 0) uniform sampler2D img;
layout(binding = 1) buffer Weights {
    float weights[N + 1]; // + norm factor
};

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec3 oColor;

void main()
{
    const vec2 tx = 1./textureSize(img, 0);
    const int n = N >> 1;
    vec2 uv = texCoord;
    vec2 duv;

    if (c_horzPass)
    {
        uv.x -= n * tx.x;
        duv = vec2(tx.x, 0);
    }
    else
    {
        uv.y -= n * tx.y;
        duv = vec2(0, tx.y);
    }

    // Gaussian filter
    oColor = vec3(0.);
    for (int i = 0; i < N; ++i, uv += duv)
        oColor += texture(img, uv).rgb * weights[i];
    oColor.rgb *= weights[N]; // normalize
}
