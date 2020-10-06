#version 450

#define N 15 // odd

layout(constant_id = 0) const bool c_horzPass = true;

layout(binding = 0) uniform sampler2D img;

layout(location = 0) out vec4 oTexCoord; // u, v, du, dv
out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    vec2 quad[4] = vec2[](
        // top
        vec2(-1.,-1.), // left
        vec2( 1.,-1.), // right
        // bottom
        vec2(-1., 1.), // left
        vec2( 1., 1.)  // right
    );
    gl_Position = vec4(quad[gl_VertexIndex], 0., 1.);
    oTexCoord.xy = gl_Position.xy * .5 + .5;
    oTexCoord.zw = 1./textureSize(img, 0);
    const int n = N >> 1;
    if (c_horzPass)
    {
        oTexCoord.x -= n * oTexCoord.z;
        oTexCoord.w = 0.; // dv
    }
    else
    {
        oTexCoord.y -= n * oTexCoord.w;
        oTexCoord.z = 0.; // du
    }
}
