vec3 phong(vec3 n, vec3 l, vec3 v,
    vec3 Ka, vec3 Ia,
    vec3 Kdiff, vec3 Idiff,
    vec3 Kspec, vec3 Ispec,
    float shininess,
    float shadow)
{
    vec3 r = reflect(-l, n);
    float NdL = dot(n, l);
    float RdV = dot(r, v);
    vec3 diff = Kdiff * Idiff;
    vec3 spec = Kspec * pow(max(RdV, 0.), shininess) * Ispec;
    return max(NdL, 0.) * (diff + spec) * shadow + Ka * Ia;
}
