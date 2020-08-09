// The facet slope distribution function D represents the fraction of the facets
// that are oriented in the direction H.

float blinnPhongDistribution(vec3 n, vec3 h, float m)
{
    float NdH = max(dot(n, h), 1e-6); // avoid division by 0
    m = 2./(m * m) - 2.;
    return (m + 2.)/(2. * PI) * pow(NdH, m);
}

float gaussianDistribution(vec3 n, vec3 h, float m)
{
    float a = acos(dot(n, h)); // involves PI
    float x = a/m;
    float c = 1.7; // resembles Blinn/Beckmann
    return c * exp(-(x * x));
}

float beckmannDistribution(vec3 n, vec3 h, float m)
{
    float NdH = max(dot(n, h), 1e-3); // avoid division by 0
    float NdH_2 = NdH * NdH;
    float m_2_NdH_2 = m * m * NdH_2;
    float e = exp((NdH_2 - 1.)/(m_2_NdH_2));
    return e/(PI * m_2_NdH_2 * NdH_2); // pi*m^2*(n.h)^4
}
