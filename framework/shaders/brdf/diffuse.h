float halfcosPow5(float cosTheta)
{
    float x = 1. - cosTheta/2.;
    float x_2 = x * x;
    return x_2 * x_2 * x;
}

// https://pdfs.semanticscholar.org/808d/048d4331cf5dac5fd924b50249b7915c6d73.pdf
vec3 ashikhminShirleyDiffuse(vec3 albedo, float f0, float NdL, float NdV)
{
    return 28./23. * albedo/PI * (1. - f0) *
        (1. - halfcosPow5(NdL)) *
        (1. - halfcosPow5(NdV));
}
