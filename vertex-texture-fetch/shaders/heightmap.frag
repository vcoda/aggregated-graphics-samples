#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/noise2d.h"

#define ITERATIONS 5

layout(constant_id = 0) const float c_invWidth = 1.;
layout(constant_id = 1) const float c_invHeight = 1.;
layout(constant_id = 2) const float c_height = 0.;
layout(constant_id = 3) const float c_choppy = 0.;
layout(constant_id = 4) const float c_speed = 0.;
layout(constant_id = 5) const float c_frequency = 0.;

layout(binding = 0) uniform Time {
    float time;
};

layout(location = 0) out float oHeight;

float octave(vec2 uv, float choppy)
{
    uv += wavenoise(uv);
    vec2 wv = 1. - abs(sin(uv));
    vec2 swv = abs(cos(uv));
    wv = mix(wv, swv, wv);
    return pow(1. - pow(wv.x * wv.y, 0.65), choppy);
}

// Seascape shader: https://www.shadertoy.com/view/Ms2SD1
void main()
{
    const mat2 octaveMat = mat2(
        1.6, 1.2,
       -1.2, 1.6);
    float freq = c_frequency;
    float amp = c_height;
    float choppy = c_choppy;
    float t = 1. + time * c_speed;
    vec2 uv = gl_FragCoord.xy * vec2(c_invWidth, c_invHeight); // [0,1]

    oHeight = 0.;
    for (int i = 0; i < ITERATIONS; ++i)
    {
        float d = octave((uv + t) * freq, choppy);
        d +=      octave((uv - t) * freq, choppy);
        oHeight += d * amp;
        uv *= octaveMat;
        freq *= 1.9;
        amp *= 0.22;
        choppy = mix(choppy, 1.0, 0.2);
    }
}
