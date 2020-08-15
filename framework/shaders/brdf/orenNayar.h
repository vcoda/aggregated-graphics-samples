vec3 orenNayar(vec3 n, vec3 l, vec3 v,
    vec3 Ka, vec3 Ia,
    vec3 Kdiff, vec3 Idiff,
    float roughness,
    float shadow)
{
    float NdL = dot(n, l);
    float NdV = dot(n, v);
    float LdV = dot(l, v);
    float a = max(NdL, NdV);
    float b = min(NdL, NdV);
    float g = dot(v - n * NdV, l - n * NdL);
    float m2 = roughness * roughness;
    float A = 1. - .5 * m2/(m2 + .33);
    float B = .45 * m2/(m2 + .09);
    float radiance = A + B * max(g, 0.) * sqrt((1. - a * a) * (1. - b * b))/a;
    return Ka * Ia + Kdiff * max(NdL, 0.) * radiance * Idiff * shadow;
}
