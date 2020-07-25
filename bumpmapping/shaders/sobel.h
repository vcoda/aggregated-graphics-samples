vec3 sobel(sampler2D heightMap, vec2 uv, float scale)
{
    mat3 h3x3 = mat3(
        textureOffset(heightMap, uv, ivec2(1, -1)).x,
        textureOffset(heightMap, uv, ivec2(1, 0)).x,
        textureOffset(heightMap, uv, ivec2(1, 1)).x,
        textureOffset(heightMap, uv, ivec2(0, -1)).x,
        0,
        textureOffset(heightMap, uv, ivec2(0, 1)).x,
        textureOffset(heightMap, uv, ivec2(-1, -1)).x,
        textureOffset(heightMap, uv, ivec2(-1, 0)).x,
        textureOffset(heightMap, uv, ivec2(-1, 1)).x);
    // http://en.wikipedia.org/wiki/Sobel_operator
    //        1 0 -1      1  2  1
    //    X = 2 0 -2  Y = 0  0  0
    //        1 0 -1     -1 -2 -1
    vec3 normal;
    normal.x = dot(h3x3 * vec3(-1, 0, 1), vec3(1, 2, 1));
    normal.y = dot(h3x3 * vec3(1, 2, 1), vec3(1, 0, -1));
    normal.z = 1./scale;
    return normalize(normal);
}
