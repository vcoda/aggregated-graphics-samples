// https://en.wikipedia.org/wiki/Beer%E2%80%93Lambert_law
// https://www.pvlighthouse.com.au/cms/lectures/altermatt/optics/the-lambert-beer-law
// https://www.rp-photonics.com/absorbance.html
// https://www.rp-photonics.com/absorption_length.html

vec3 absorb(vec3 rgb, float c, float x)
{
    vec3 a = -log(rgb) * c;
    return exp(-a * x);
}
