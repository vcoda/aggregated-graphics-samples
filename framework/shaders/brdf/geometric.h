float implicitGeometric(vec3 n, vec3 h, vec3 v, vec3 l, float)
{
    float NdL = max(dot(n, l), 0.);
    float NdV = max(dot(n, v), 0.);
    return NdL * NdV;
}

float cookTorranceGeometric(vec3 n, vec3 h, vec3 v, vec3 l, float)
{
    float NdH = dot(n, h);
    float NdV = dot(n, v);
    float NdL = dot(n, l);
    float VdH = dot(v, h);
    float G = 2. * NdH/VdH * min(NdV, NdL);
    return min(1., max(G, 0.));
}

float schlickGeometric(vec3 n, vec3 h, vec3 v, vec3 l, float m)
{
    float NdL = max(dot(n, l), 0.);
    float NdV = max(dot(n, v), 0.);
    float k = m * sqrt(2./PI);
    return (NdL/(NdL * (1. - k) + k)) * (NdV/(NdV * (1. - k) + k));
}
