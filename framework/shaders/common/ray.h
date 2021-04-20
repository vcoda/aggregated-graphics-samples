// https://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld017.htm
float rayPlaneIntersection(vec4 p, vec3 o, vec3 d)
{
    return -(dot(o, p.xyz) + p.w)/dot(d, p.xyz);
}
