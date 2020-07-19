#pragma once
#include "core/noncopyable.h"
#include "rapid/rapid.h"

/*
    ARCBALL: A User Interface for Specifying
    Three-Dimensional Orientation Using a Mouse
    Ken Shoemake
    Computer Graphics Laboratory
    University of Pennsylvania
    https://www.talisman.org/~erlkonig/misc/shoemake92-arcball.pdf
*/

class Arcball : public core::NonCopyable
{
public:
    Arcball(const rapid::vector2& center, float radius,
        bool rhs = true);
    void touch(const rapid::vector2& pos);
    void release();
    void rotate(const rapid::vector2& pos);
    void reset();
    void buildArc(uint32_t lgNumSegs, rapid::float2 *arc) const;

    rapid::matrix transform() const { return rapid::matrix(curr); }
    const rapid::vector2& center() const { return c; }
    float radius() const { return r; }
    bool isTouched() const { return touched; }
    bool isDragged() const { return dragged; }

private:
    void update(const rapid::vector2& pos);
    virtual rapid::quaternion rotation(const rapid::vector3& from,
        const rapid::vector3& to) const;

private:
    rapid::vector2 c;
    float r;
    bool rhs;
    bool touched, started, dragged;
    rapid::quaternion curr, last;
    rapid::vector2 from;
    rapid::vector3 projFrom, projTo;
};

class Trackball : public Arcball
{
public:
    Trackball(const rapid::vector2& center, float radius,
        bool rhs = true);

private:
    virtual rapid::quaternion rotation(const rapid::vector3& from,
        const rapid::vector3& to) const override;
};
