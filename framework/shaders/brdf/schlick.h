// https://en.wikipedia.org/wiki/Schlick%27s_approximation
float fresnelSchlick(float f0, float cosTheta)
{
    float x = 1. - cosTheta;
    return f0 + (1. - f0) * x * x * x * x * x;
}

// some material like copper and gold have different f0 term in RGB channels
vec3 fresnelSchlick(vec3 f0, float cosTheta)
{
    float x = 1. - cosTheta;
    return f0 + (vec3(1.) - f0) * x * x * x * x * x;
}

float fresnelSchlick(float n1 /* left */, float n2 /* entered */, vec3 n, vec3 i)
{
    float f0 = (n1 - n2)/(n1 + n2);
    return fresnelSchlick(f0 * f0, dot(n, -i));
}
