// https://www.shadertoy.com/view/lsf3WH
float hash(vec2 p)
{
    p = 50. * fract(p * 0.3183099 + vec2(0.71, 0.113));
    return -1. + 2. * fract(p.x * p.y * (p.x + p.y));
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3. - 2. * f);
    return mix(mix(hash(i + vec2(0., 0.)),
                   hash(i + vec2(1., 0.)), u.x),
               mix(hash(i + vec2(0., 1.)),
                   hash(i + vec2(1., 1.)), u.x), u.y);
}