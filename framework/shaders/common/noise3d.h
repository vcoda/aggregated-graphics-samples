// https://www.shadertoy.com/view/4sfGzS
float hash(vec3 p)
{
    p  = fract(p * 0.3183099 + .1);
    p *= 17.0;
    float sum = dot(p, vec3(1.));
    return fract(p.x * p.y * p.z * sum);
}

float noise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3. - 2. * f);
    return mix(mix(mix(hash(i + vec3(0, 0, 0)),
                       hash(i + vec3(1, 0, 0)), u.x),
                   mix(hash(i + vec3(0, 1, 0)),
                       hash(i + vec3(1, 1, 0)), u.x), u.y),
               mix(mix(hash(i + vec3(0, 0, 1)),
                       hash(i + vec3(1, 0, 1)), u.x),
                   mix(hash(i + vec3(0, 1, 1)),
                       hash(i + vec3(1, 1, 1)), u.x), u.y), u.z);
}
