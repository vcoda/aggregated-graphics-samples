#include "common/transforms.h"
#include "common/sRGB.h"

layout(binding = 2) uniform Light {
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

layout(constant_id = 0) const bool c_gammaCorrection = true;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 viewPos;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 viewNormal;
layout(location = 4) in vec2 texCoord;

layout(location = 0) out vec3 oColor;

void sRgbFix()
{
    // revert x^1/2.2 in the framebuffer
    if (!c_gammaCorrection)
        oColor = linear(oColor);
}
