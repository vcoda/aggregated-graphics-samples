#include "common/poisson32.h"

float pcf(sampler2DShadow shadowMap, vec4 clipPos, float bias, mat2 jitter)
{
    clipPos.z += bias;
    float sum = 0.;
    for (int i = 0; i < 32; ++i)
    {
        vec2 offset = jitter * poisson32[i];
        sum += textureProj(shadowMap, vec4(clipPos.xy + offset, clipPos.zw));
    }
    return sum/32.;
}
