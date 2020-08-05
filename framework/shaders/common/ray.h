// https://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld017.htm
float rayPlane(vec4 p, vec3 ro, vec3 rd)
{
    return -(dot(ro, p.xyz) + p.w)/dot(rd, p.xyz);
}
