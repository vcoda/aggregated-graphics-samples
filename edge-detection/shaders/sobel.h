float sobel(sampler2D depthMap, vec2 uv)
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
        textureGatherOffsets(depthMap, uv, offsets0),
        textureGatherOffsets(depthMap, uv, offsets1));
    mat3 d3x3 = mat3(
        h[0].xyz,
        vec3(h[0].w, 0, h[1].x),
        h[1].yzw);
    // http://en.wikipedia.org/wiki/Sobel_operator
    //        1 0 -1      1  2  1
    //    X = 2 0 -2  Y = 0  0  0
    //        1 0 -1     -1 -2 -1
    vec2 G;
    G.x = dot(d3x3 * vec3(-1, 0, 1), vec3(1, 2, 1));
    G.y = dot(d3x3 * vec3(1, 2, 1), vec3(1, 0, -1));
    return length(G);
}
