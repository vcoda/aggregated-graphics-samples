vec3 blinnPhong(vec3 n, vec3 l, vec3 v,
    vec3 Ka, vec3 Ia,
    vec3 Kdiff, vec3 Idiff,
    vec3 Kspec, vec3 Ispec,
    float shininess,
    float shadow)
{
    vec3 h = normalize(l + v);
    float NdL = max(dot(n, l), 0.);
    float NdH = max(dot(n, h), 0.);
    return Ka * Ia + shadow * (Kdiff * NdL * Idiff) + (Kspec * pow(NdH, shininess) * Ispec);
}
