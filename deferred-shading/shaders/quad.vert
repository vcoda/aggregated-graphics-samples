#version 450

layout(location = 0) out vec2 oScreenPos;
layout(location = 1) out vec2 oTexCoord;
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
    oScreenPos = quad[gl_VertexIndex];
    oTexCoord = oScreenPos * .5 + .5;
    gl_Position = vec4(oScreenPos, 0., 1.);
}
