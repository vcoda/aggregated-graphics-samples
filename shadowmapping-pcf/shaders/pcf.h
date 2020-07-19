float pcf(sampler2DShadow shadowMap, vec4 clipPos)
{
    const int radius = 3;
    float sum = 0.;
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-2, -2) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-1, -2) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(0, -2) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(1, -2) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(2, -2) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-2, -1) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-1, -1) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(0, -1) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(1, -1) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(2, -1) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-2, 0) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-1, 0) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(0, 0) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(1, 0) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(2, 0) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-2, 1) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-1, 1) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(0, 1) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(1, 1) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(2, 1) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-2, 2) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-1, 2) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(0, 2) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(1, 2) * radius);
    sum += textureProjOffset(shadowMap, clipPos, ivec2(2, 2) * radius);
    return sum/25.;
}
