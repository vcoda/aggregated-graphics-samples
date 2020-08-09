#include "geometric.h"
#include "distribution.h"
#include "schlick.h"

float diffuseEnergyRatio(vec3 rgbF0, float cosTheta)
{
    float f0 = dot(rgbF0, vec3(0.299, 0.587, 0.114)); // ITU BT.601
#ifdef E_1_FDIFF
    return 1. - fresnelSchlick(f0, cosTheta);
#else
    return 1. - f0;
#endif
}

vec3 cookTorrance(vec3 n, vec3 l, vec3 v,
    vec3 f0, float roughness,
    vec3 Kalbedo, vec3 Idiff)
{
    vec3 h = normalize(l + v);
    vec3 F = fresnelSchlick(f0, dot(h, v));
#if defined(D_BLINN_PHONG)
    float D = blinnPhongDistribution(n, h, roughness);
#elif defined(D_GAUSSIAN)
    float D = gaussianDistribution(n, h, roughness);
#elif defined(D_BECKMANN)
    float D = beckmannDistribution(n, h, roughness);
#else
#error normal distribution function not defined
#endif
#if defined(G_IMPLICIT)
    float G = implicitGeometric(n, h, v, l, roughness);
#elif defined(G_COOK_TORRANCE)
    float G = cookTorranceGeometric(n, h, v, l, roughness);
#elif defined(G_SCHLICK)
    float G = schlickGeometric(n, h, v, l, roughness);
#else
#error geometric function not defined
#endif
    float NdL = dot(n, l);
    float NdV = dot(n, v);
    vec3 spec = F * D * G/max(4. * NdL * NdV, 1e-6);
    vec3 diff = Kalbedo * diffuseEnergyRatio(f0, NdL);
    return Idiff * max(NdL, 0.) * vec3(diff + spec);
}
