#version 450
#extension GL_GOOGLE_include_directive : enable
#include "common/transforms.h"
#include "common/ray.h"
#include "common/absorption.h"
#include "common/ior.h"
#include "brdf/schlick.h"
#include "sobel.h"

layout(binding = 2) uniform DirectionalLight {
    vec4 viewDir;
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

layout(binding = 4) uniform Seabed {
    vec4 viewPlane;
} seabed;

layout(binding = 5) uniform sampler2D heightMap;
layout(binding = 6) uniform samplerCube envMap;

layout(location = 0) in vec3 viewPos;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec3 oColor;

void main()
{
    // reconstruct normal from height map
    const float strength = 10.;
    vec3 normal = sobel(heightMap, texCoord, strength);

    // view-space ray
    const vec3 eye = vec3(0);
    vec3 i = normalize(viewPos - eye);

    // calculate absorption path length
    float seabedDistance = rayPlane(seabed.viewPlane, eye, i);
    float surfaceDistance = distance(eye, viewPos);
    float absorptionLength = seabedDistance - surfaceDistance;

    // calculate refracted water color
    vec3 sigma = attenuationCoefficient(surface.diffuse.rgb, 0.1);
    vec3 refracted = absorb(sigma, absorptionLength);

    vec3 l = light.viewDir.xyz;
    vec3 n = mat3(normalMatrix) * normal.xzy; // swap Y, Z
    vec3 r = reflect(-l, n);

    // calcular specular
    float RdV = max(dot(r, -i), 0.);
    vec3 specular = surface.specular.rgb * pow(RdV, surface.shininess) * light.specular.rgb;

    // lookup cubemap for reflected color
    vec3 reflected = texture(envMap, reflect(i, n)).rgb * 1.2;

    float coeff = fresnelSchlick(IOR_AIR, IOR_WATER, n, i);
    oColor = mix(refracted, reflected, coeff) + specular;
}
