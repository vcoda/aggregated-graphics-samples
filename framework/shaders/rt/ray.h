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

Ray raycast(mat4 viewInv, mat4 viewProjInv)
{
    vec2 screenPos = gl_LaunchIDNV.xy + 0.5;
    vec3 ndcPos = vec3(screenPos / gl_LaunchSizeNV.xy * 2. - 1., 1.);
    Ray ray;
    ray.origin = (viewInv * vec4(0, 0, 0, 1)).xyz;
    ray.dir = (viewProjInv * vec4(ndcPos, 1.)).xyz;
    ray.dir = normalize(ray.dir);
    return ray;
}
