#pragma once
#include "color.h"

struct alignas(16) Transforms
{
    rapid::matrix world;
    rapid::matrix worldInv;
    rapid::matrix worldView;
    rapid::matrix worldViewProj;
    rapid::matrix worldLightProj;
    rapid::matrix normal; // gl_NormalMatrix = transpose(inverse(gl_ModelViewMatrix))
};

struct alignas(16) RtTransforms
{
    rapid::matrix world;
    rapid::matrix normal; // gl_NormalMatrix = transpose(inverse(gl_ModelViewMatrix))
};

struct alignas(16) ViewProjTransforms
{
    rapid::matrix view;
    rapid::matrix viewInv;
    rapid::matrix proj;
    rapid::matrix projInv;
    rapid::matrix viewProj;
    rapid::matrix viewProjInv;
    rapid::matrix shadowProj;
};

struct alignas(16) LightSource
{
    union {
        rapid::vector viewPosition;
        rapid::vector viewDirection;
    };
    LinearColor ambient;
    LinearColor diffuse;
    LinearColor specular;
};

struct alignas(16) PhongMaterial
{
    LinearColor ambient;
    LinearColor diffuse;
    LinearColor specular;
    float shininess;
};

struct DescriptorSet
{
    std::shared_ptr<magma::DescriptorSetLayout> layout;
    std::shared_ptr<magma::DescriptorSet> set;
};
