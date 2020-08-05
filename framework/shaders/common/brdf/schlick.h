// https://en.wikipedia.org/wiki/Schlick%27s_approximation
float fresnelSchlick(float n1 /* left */, float n2 /* entered */, vec3 N, vec3 I)
{
    float r0 = (n1 - n2)/(n1 + n2);
    r0 *= r0;
    float theta = -dot(N, I);
    float x = 1. - theta;
    return r0 + (1. - r0) * x * x * x * x * x;
}
