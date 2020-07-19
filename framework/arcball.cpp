#include "arcball.h"
#include <memory>
#include "magma/core/core.h"
#include "magma/helpers/stackArray.h"

Arcball::Arcball(const rapid::vector2& center, float radius, bool rhs /* true */):
    c(center), r(radius), rhs(rhs)
{
    reset();
}

void Arcball::touch(const rapid::vector2& pos)
{
    if (!(pos - c) < r)
        touched = true;
}

void Arcball::release()
{
    touched = started = dragged = false;
    projFrom.zero();
    projTo.zero();
}

void Arcball::rotate(const rapid::vector2& pos)
{
    update(pos);
    if (isDragged())
    {
        const rapid::vector2 unitFrom = (from - c)/r;
        const rapid::vector2 unitTo = (pos - c)/r;
        if (rhs)
        {
            projFrom = rapid::orthoProjectOnSphereRH(unitFrom);
            projTo = rapid::orthoProjectOnSphereRH(unitTo);
        }
        else // lhs
        {
            projFrom = rapid::orthoProjectOnSphereLH(unitFrom);
            projTo = rapid::orthoProjectOnSphereLH(unitTo);
        }
        curr = rotation(projFrom, projTo);
        curr = last * curr;
    }
}

void Arcball::reset()
{
    touched = started = dragged = false;
    curr.identity();
    last.identity();
    projFrom.zero();
    projTo.zero();
}

void Arcball::buildArc(uint32_t lgNumSegs, rapid::float2 *arc) const
{
    rapid::vector3 a = projFrom;
    rapid::vector3 b = projTo;
    for (uint32_t i = 0; i < lgNumSegs; ++i)
    {
        b += a;
        if (!b < rapid::constants::epsilon)
            b.zero();
        else
            b.normalize();
    }
    const uint32_t numSegs = 1 << lgNumSegs;
    MAGMA_STACK_ARRAY(rapid::vector3, v, numSegs + 1);
    const rapid::vector3 dp2 = (a | b) * 2.f;
    v[0] = projFrom;
    v[1] = b;
    uint32_t i;
    for (i = 2; i < numSegs; ++i)
        v[i] = v[i - 1] * dp2 - v[i - 2];
    v[numSegs] = projTo;
    for (i = 0; i < numSegs + 1; ++i)
        v[i].store(&arc[i]);
}

void Arcball::update(const rapid::vector2& pos)
{
    if (!started && touched)
    {
        from = pos;
        last = curr.normalized();
        started = true;
    }
    else if (started)
    {
        if (touched)
            dragged = true;
        else
        {
            started = dragged = false;
            projFrom.zero();
            projTo.zero();
        }
    }
}

rapid::quaternion Arcball::rotation(const rapid::vector3& from, const rapid::vector3& to) const
{
    rapid::quaternion q;
    rapid::vector3 c = from ^ to;
    if (c.length() <= rapid::constants::epsilon)
        q.identity();
    else
        q = rapid::quaternion(c, from | to);
    return q;
}

Trackball::Trackball(const rapid::vector2& center, float radius, bool rhs /* true */):
    Arcball(center, radius, rhs)
{}

rapid::quaternion Trackball::rotation(const rapid::vector3& from, const rapid::vector3& to) const
{   // shortest arc
    const rapid::vector3 c = from ^ to;
    rapid::quaternion q(c.x(), c.y(), c.z(), from | to);
    q.normalize(); // if <from> or <to> not unit, normalize quaternion
    float w = q.w() + 1.f; // reduce angle to halfangle
    if (w > rapid::constants::epsilon) // angle close to PI
        q = rapid::quaternion(q.xyz(), w);
    else
    {
        rapid::float3a f;
        from.store(&f);
        if (f.z * f.z > f.x * f.x)
            q = rapid::quaternion(0, f.z, -f.y, w);
        else
            q = rapid::quaternion(f.y, -f.x, 0.f, w);
    }
    return q.normalized();
}
