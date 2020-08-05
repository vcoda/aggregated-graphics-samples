vec3 sobel(sampler2D heightMap, vec2 uv, float scale)
{
    const ivec2 offsets0[] = ivec2[4](
        ivec2(1, -1),
        ivec2(1, 0),
        ivec2(1, 1),
        ivec2(0, -1));
    const ivec2 offsets1[] = ivec2[4](
        ivec2(0, 1),
        ivec2(-1, -1),
        ivec2(-1, 0),
        ivec2(-1, 1));
    vec4 h[2] = vec4[2](
        textureGatherOffsets(heightMap, uv, offsets0),
        textureGatherOffsets(heightMap, uv, offsets1));
    mat3 h3x3 = mat3(
        h[0].xyz,
        vec3(h[0].w, 0, h[1].x),
        h[1].yzw);
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
