#include "viewProjection.h"

ViewProjection::ViewProjection(bool lhs) noexcept:
    lhs(lhs),
    eyePos(0.f, 0.f, -1.f),
    focusPos(0.f, 0.f, 0.f)
{}

void ViewProjection::translate(float x, float y, float z) noexcept
{
    eyePos.x += x;
    eyePos.y += y;
    eyePos.z += z;
}

void ViewProjection::updateView() noexcept
{
    const rapid::vector3 up(0.f, 1.f, 0.f);
    rapid::vector3 eye(eyePos);
    const float cosTheta = up.dot(eye.normalized());
    if (cosTheta > 0.999f)
    {
        constexpr float eps = std::numeric_limits<float>::epsilon();
        eye += rapid::vector3(-eps, 0.f, eps);
    }
    const rapid::vector3 focus(focusPos);
    view = leftHanded() ? rapid::lookAtLH(eye, focus, up)
                        : rapid::lookAtRH(eye, focus, up);
    viewInv = rapid::inverse(view);
}

void ViewProjection::updateProjection(bool flipY /* true */) noexcept
{   // Resulting projection considers the particularities of how the Vulkan
    // coordinate system is defined (Y axis is inversed, Z range is halved).
    const rapid::matrix flip(
        1.f, 0.f, 0.f, 0.f,
        0.f,-1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f);
    const float fov = rapid::radians(fieldOfView);
    proj = leftHanded() ? rapid::perspectiveFovLH(fov, aspectRatio, zNear, zFar)
                        : rapid::perspectiveFovRH(fov, aspectRatio, zNear, zFar);
    if (flipY)
        proj = proj * flip;
    projInv = rapid::inverse(proj);
    viewProj = view * proj;
}

rapid::matrix ViewProjection::calculateNormal(const rapid::matrix& world) const noexcept
{   // gl_NormalMatrix = transpose(inverse(gl_ModelViewMatrix))
    const rapid::matrix worldView = world * view;
    const rapid::matrix worldViewInv = rapid::inverse(worldView);
    const rapid::matrix normal = rapid::transpose(worldViewInv);
    return normal;
}

rapid::matrix ViewProjection::calculateShadowProj() const noexcept
{   // (x,y) [-1,1] -> [0,1]
    const rapid::matrix bias(
        .5f, .0f, 0.f, 0.f,
        .0f, .5f, 0.f, 0.f,
        .0f, .0f, 1.f, 0.f,
        .5f, .5f, 0.f, 1.f);
    const rapid::matrix shadowProj = viewProj * bias;
    return shadowProj;
}
