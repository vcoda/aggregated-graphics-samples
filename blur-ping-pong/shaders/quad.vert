#version 450

layout(constant_id = 0) const bool c_ping = true;

layout(binding = 0) uniform sampler2D img;

layout(location = 0) out vec2 oTexCoord;
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
    const vec2 off = 0.5/textureSize(img, 0); // half texel offset
    gl_Position = vec4(quad[gl_VertexIndex], 0., 1.);
    oTexCoord = gl_Position.xy * .5 + .5;
    oTexCoord += c_ping ? off : -off;
}
