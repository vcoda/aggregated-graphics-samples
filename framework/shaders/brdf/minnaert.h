vec3 minnaert(vec3 n, vec3 l, vec3 v,
    vec3 Kdiff, vec3 Idiff,
    float k)
{
    float NdL = max(dot(n, l), 0.);
    float NdV = max(dot(n, v), 0.);
    return Kdiff * pow(NdL * NdV, k - 1.) * Idiff;
}
