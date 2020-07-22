#pragma once
#include "core/aligned.h"
#include "core/noncopyable.h"
#include "rapid/rapid.h"

class ViewProjection : public core::Aligned<16>, public core::NonCopyable
{
protected:
    ViewProjection(bool lhs) noexcept;

public:
    bool leftHanded() const noexcept { return lhs; }
    void translate(float x, float y, float z) noexcept;
    void setPosition(float x, float y, float z) noexcept { eyePos = rapid::float3(x, y, z); }
    void setPosition(const rapid::float3& eyePos) noexcept { this->eyePos = eyePos; }
    const rapid::float3& getPosition() const noexcept { return eyePos; }
    void setFocus(float x, float y, float z) noexcept { focusPos = rapid::float3(x, y, z); }
    void setFocus(const rapid::float3& focusPos) noexcept { this->focusPos = focusPos; }
    const rapid::float3& getFocus() const noexcept { return focusPos; }
    void setFieldOfView(float fov) noexcept { fieldOfView = fov; }
    float getFieldOfView() const noexcept { return fieldOfView; }
    void setAspectRatio(float ratio) noexcept { aspectRatio = ratio; };
    float getAspectRatio() const noexcept { return aspectRatio; }
    void setNearZ(float z) noexcept { zNear = z; }
    float getNearZ() const noexcept { return zNear; }
    void setFarZ(float z) noexcept { zFar = z; }
    float getFarZ() const noexcept { return zFar; }
    const rapid::matrix& getView() const noexcept { return view; }
    const rapid::matrix& getViewInv() const noexcept { return viewInv; }
    const rapid::matrix& getProj() const noexcept { return proj; }
    const rapid::matrix& getProjInv() const noexcept { return projInv; }
    const rapid::matrix& getViewProj() const noexcept { return viewProj; }
    void updateView() noexcept;
    void updateProjection(bool flipY = true) noexcept;
    rapid::matrix calculateNormal(const rapid::matrix& world) const noexcept;
    rapid::matrix calculateShadowProj() const noexcept;

private:
    bool lhs;
    rapid::float3 eyePos;
    rapid::float3 focusPos;
    float fieldOfView = 45.f;
    float aspectRatio = 1.f;
    float zNear = 0.1f;
    float zFar = 100.f;
    rapid::matrix view;
    rapid::matrix viewInv;
    rapid::matrix proj;
    rapid::matrix projInv;
    rapid::matrix viewProj;
    rapid::matrix normal;
};

/* Left-handed coordinate system used in DirectX, Metal, Vulkan. */

class LeftHandedViewProjection : public ViewProjection
{
public:
    LeftHandedViewProjection() noexcept:
        ViewProjection(true) {}
};

/* Right-handed coordinate system used in OpenGL, GLM. */

class RightHandedViewProjection : public ViewProjection
{
public:
    RightHandedViewProjection() noexcept:
        ViewProjection(false) {}
};
