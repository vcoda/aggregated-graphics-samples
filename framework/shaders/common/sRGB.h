// https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_sRGB.txt
float sRGB(float cl)
{
    if (cl > 1.)
        return 1.;
    if (cl < 0.)
        return 0.;
    if (cl < 0.0031308)
        return 12.92 * cl;
    return 1.055 * pow(cl, 1.0/2.4) - 0.055;
}

vec3 sRGB(vec3 cl)
{
    return vec3(sRGB(cl.r), sRGB(cl.g), sRGB(cl.b));
}

float linear(float cs)
{
    cs = clamp(cs, 0., 1.);
    if (cs <= 0.04045)
        return cs/12.92;
    return pow((cs + 0.055)/1.055, 2.4);
}

vec3 linear(vec3 cs)
{
    return vec3(linear(cs.r), linear(cs.g), linear(cs.b));
}
