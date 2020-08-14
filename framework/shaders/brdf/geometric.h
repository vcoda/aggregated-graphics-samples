float implicitGeometric(vec3 n, vec3 h, vec3 v, vec3 l, float)
{
    float NdL = max(dot(n, l), 0.);
    float NdV = max(dot(n, v), 0.);
    return NdL * NdV;
}

float cookTorranceGeometric(vec3 n, vec3 h, vec3 v, vec3 l, float)
{
    float NdH2 = 2. * dot(n, h);
    float NdV = dot(n, v);
    float NdL = dot(n, l);
    float VdH = max(dot(v, h), 1e-6); // avoid division by 0
    return min(1., min(NdH2 * NdV/VdH, NdH2 * NdL/VdH));
}

float schlickGeometric(vec3 n, vec3 h, vec3 v, vec3 l, float m)
{
    float NdL = max(dot(n, l), 0.);
    float NdV = max(dot(n, v), 0.);
    float k = m * sqrt(2./PI);
    return (NdL/(NdL * (1. - k) + k)) * (NdV/(NdV * (1. - k) + k));
}
