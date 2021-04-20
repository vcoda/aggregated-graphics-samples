vec3 blinnPhong(vec3 n, vec3 l, vec3 v,
    vec3 Ka, vec3 Ia,
    vec3 Kdiff, vec3 Idiff,
    vec3 Kspec, vec3 Ispec,
    float shininess,
    float shadow)
{
    vec3 h = normalize(l + v);
    float NdL = dot(n, l);
    float NdH = dot(n, h);
    vec3 diff = Kdiff * max(NdL, 0.) * Idiff;
    vec3 spec = Kspec * pow(max(NdH, 0.), shininess) * Ispec;
    return (diff + spec) * shadow + Ka * Ia;
}
