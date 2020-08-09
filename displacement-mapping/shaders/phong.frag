#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/cotangentFrame.h"
#include "common/sRGB.h"
#include "brdf/phong.h"

layout(constant_id = 0) const bool c_showNormals = false;

layout(binding = 2) uniform Light {
    vec4 viewPos;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
} light;

layout(binding = 3) uniform Material {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
} surface;

layout(binding = 4) uniform Parameters {
    vec4 screenSize;
    float displacement;
};

layout(binding = 5) uniform sampler2D displacementMap;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 viewPos;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texCoord;

layout(location = 0) out vec3 oColor;

void main()
{
    vec3 micronormal = texture(displacementMap, texCoord).xyz;

    // compute per-pixel cotangent frame
    vec3 N = normalize(normal);
    mat3 TBN = cotangentFrame(N, position, texCoord);

    vec3 l = normalize(light.viewPos.xyz - viewPos);
    vec3 v = -normalize(viewPos);

    // transform from texture space to object space
    micronormal = TBN * normalize(micronormal * 2. - 1.);
    // transform from object space to view space
    vec3 n = mat3(normalMatrix) * micronormal;
    n = normalize(n);

    oColor = phong(n, l, v,
        surface.ambient.rgb, light.ambient.rgb,
        surface.diffuse.rgb, light.diffuse.rgb,
        surface.specular.rgb, light.specular.rgb,
        surface.shininess, 1.);

    if (c_showNormals)
    {
        if (gl_FragCoord.x > screenSize.x * 0.5)
            oColor = linear(micronormal * .5 + .5);
    }
}
