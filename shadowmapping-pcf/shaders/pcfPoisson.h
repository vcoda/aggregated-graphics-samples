float pcfPoisson(sampler2DShadow shadowMap, vec4 clipPos)
{
    float sum = 0.;
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-3, 4));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-3, -3));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(1, -5));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(4, 4));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(2, -1));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(6, 2));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(1, 5));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-4, 1));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-3, -6));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-1, 1));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(3, -6));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(6, -1));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-2, 6));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-0, -2));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-5, 5));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(2, 2));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(4, 1));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(3, -3));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(3, 6));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-5, -2));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-6, 2));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-1, 4));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-1, -7));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-7, 0));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-5, -5));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-3, -1));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-3, 2));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(1, 1));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(2, -2));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(1, 7));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(-1, -5));
    sum += textureProjOffset(shadowMap, clipPos, ivec2(5, -5));
    return sum/32.f;
}
