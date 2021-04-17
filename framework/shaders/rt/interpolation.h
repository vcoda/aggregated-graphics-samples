float interpolate(vec3 abc, vec3 bary)
    { return dot(abc, bary); }

float interpolate(float a, float b, float c, vec3 bary)
    { return dot(vec3(a, b, c), bary); }

vec2 interpolate(vec2 a, vec2 b, vec2 c, vec3 bary)
    { return a * bary.x + b * bary.y + c * bary.z; }

vec3 interpolate(vec3 a, vec3 b, vec3 c, vec3 bary)
    { return a * bary.x + b * bary.y + c * bary.z; }

vec4 interpolate(vec4 a, vec4 b, vec4 c, vec3 bary)
    { return a * bary.x + b * bary.y + c * bary.z; }
