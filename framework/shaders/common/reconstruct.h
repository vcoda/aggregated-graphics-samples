vec3 reconstructNormal(vec2 xy)
{
    vec3 n;
    n.xy = xy;
    n.z = sqrt(1. - dot(xy, xy));
    return normalize(n);
}
