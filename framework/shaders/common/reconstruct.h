vec3 reconstructNormal(vec2 xy)
{
    vec3 n;
    n.xy = xy;
    n.z = sqrt(1. - dot(xy, xy));
    return normalize(n);
}

// https://mynameismjp.wordpress.com/2009/03/10/reconstructing-position-from-depth/
vec3 reconstructViewPos(vec2 xy, float z)
{
    //z = z * 2. - 1.; // skip in Vulkan
    vec4 clipPos = vec4(xy, z, 1.);
    vec4 viewPos = projInv * clipPos;
    return viewPos.xyz / viewPos.w;
}
