mat2 jitter(float a, float x, float y)
{
    float s = sin(a);
    float c = cos(a);
    mat2 rot = mat2(c,-s,
                    s, c);
    mat2 scale = mat2(x, 0,
                      0, y);
    return rot * scale;
}
