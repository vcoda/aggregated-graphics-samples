// https://en.wikipedia.org/wiki/Luma_(video)
float luma709(vec3 rgb)
{
    return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
}

float luma601(vec3 rgb)
{
    return dot(rgb, vec3(0.299, 0.587, 0.114));
}
