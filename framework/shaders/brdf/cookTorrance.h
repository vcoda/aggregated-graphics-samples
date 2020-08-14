#include "../common/luma.h"
#include "geometric.h"
#include "distribution.h"
#include "schlick.h"

vec3 cookTorrance(vec3 n, vec3 l, vec3 v,
    vec3 f0, float roughness,
    vec3 albedo, vec3 Idiff)
{
    vec3 h = normalize(l + v);
    float HdV = dot(h, v);
    float NdL = dot(n, l);
    float NdV = dot(n, v);
    vec3 F = fresnelSchlick(f0, HdV);
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
    vec3 spec = F * D * G/max(4. * NdL * NdV, 1e-6);
#if defined(F_DIFFUSE_NDL)
    vec3 Fd = fresnelSchlick(f0, NdL);
#else // UE4 computes it as dot(h, v)
    vec3 Fd = F;
#endif
    vec3 diff =  albedo * (1. - luma709(Fd))/4.;
    return Idiff * (max(NdL, 0.) * diff + spec);
}
