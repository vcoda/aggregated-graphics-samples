#version 450

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
    gl_Position = vec4(quad[gl_VertexIndex], 0., 1.);
    oTexCoord = gl_Position.xy * .5 + .5;
}
