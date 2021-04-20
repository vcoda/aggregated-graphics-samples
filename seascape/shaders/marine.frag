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
	vec3 n = mat3(normalMatrix) * normal.xzy; // swap Y, Z

    // calculate indicent and refracted rays
    vec3 i = normalize(viewPos);
	vec3 r = refract(i, n, IOR_AIR/IOR_WATER);

    // calculate absorption path length
    float absorptionLen = rayPlaneIntersection(seabed.viewPlane, viewPos, r);
    // calculate refracted water color
    const float concentration = 0.1;
    vec3 refracted = absorb(surface.diffuse.rgb, concentration, absorptionLen);

    // lookup cubemap for reflected color
    r = reflect(i, n);
    vec3 reflected = texture(envMap, r).rgb * 1.2;
    
    // calculate diffuse color
    float coeff = fresnelSchlick(IOR_AIR, IOR_WATER, n, i);
    vec3 diffuse = mix(refracted, reflected, coeff);

    // calculate reflected ray
    vec3 l = light.viewDir.xyz;
    r = reflect(-l, n);
    // calculate specular color
    vec3 v = -i;
    float RdV = max(dot(r, v), 0.);
    vec3 specular = surface.specular.rgb * pow(RdV, surface.shininess) * light.specular.rgb;
   
    oColor = diffuse + specular;
}
