vec3 phong(vec3 n, vec3 l, vec3 v,
    vec3 Ka, vec3 Ia,
    vec3 Kdiff, vec3 Idiff,
    vec3 Kspec, vec3 Ispec,
    float shininess,
    float shadow)
{
    float NdL = max(dot(n, l), 0.);
    vec3 r = reflect(-l, n);
    float RdV = max(dot(r, v), 0.);
    return Ka * Ia + shadow * (Kdiff * NdL * Idiff) + (Kspec * pow(RdV, shininess) * Ispec);
}
