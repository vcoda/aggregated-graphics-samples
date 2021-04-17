struct Ray
{
    vec3 origin;
    vec3 dir;
};

Ray raycast(float fov)
{
    vec2 screenPos = gl_LaunchIDNV.xy + 0.5;
    vec2 pos = screenPos/gl_LaunchSizeNV.xy * 2. - 1.;
    Ray ray;
    ray.origin = vec3(pos.x, -pos.y, -1.);
    ray.dir = normalize(vec3(ray.origin.xy * tan(fov * 0.5), 1.));
    return ray;
}
