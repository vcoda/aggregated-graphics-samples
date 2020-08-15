// Bruce Walter, "Notes on the Ward BRDF", Technical Report PCG-05-06, 2005.

float sqr(float x)
{
    return x * x;
}

vec3 ward(vec3 n, vec3 l, vec3 v,
    vec3 x, vec3 y,
    float ax, float ay,
    vec3 Kdiff, vec3 Idiff,
    vec3 Kspec, vec3 Ispec)
{
    vec3 h = normalize(l + v);
    float NdH = dot(n, h);
    float NdL = dot(n, l);
    float NdV = dot(n, v);
    float e = (sqr(dot(h, x)/ax) + sqr(dot(h, y)/ay))/(NdH * NdH);
    float spec = 1./(4. * PI * ax * ay * sqrt(NdL * NdV)) * exp(-e);
    return Kdiff * max(NdL, 0.) * Idiff + Kspec * spec * Ispec;
}
