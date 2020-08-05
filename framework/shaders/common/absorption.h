// https://en.wikipedia.org/wiki/Beer%E2%80%93Lambert_law
// https://www.pvlighthouse.com.au/cms/lectures/altermatt/optics/the-lambert-beer-law
// https://www.rp-photonics.com/absorption_length.html

vec3 absorb(vec3 a, float l)
{
    return exp(-a * l);
}

vec3 attenuationCoefficient(vec3 c, float d)
{
    return -log(c) * d;
}
