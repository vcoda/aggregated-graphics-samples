#define PI 3.14159265359
#define TWO_PI (2. * PI)

layout(binding = 0, set = 0) uniform Transforms
{
    mat4 world;
    mat4 worldInv;
    mat4 worldView;
    mat4 worldViewProj;
    mat4 worldLightProj;
    mat4 normalMatrix;
};

layout(binding = 1, set = 0) uniform ViewProjTransforms
{
    mat4 view;
    mat4 viewInv;
    mat4 proj;
    mat4 projInv;
    mat4 viewProj;
    mat4 viewProjInv;
    mat4 shadowProj;
};
