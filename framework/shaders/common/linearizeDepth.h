float linearizeDepth(float depth, float zn, float zf)
{
    return (2. * zn)/(zf + zn - depth * (zf - zn));
}
