#pragma once
#include "rapid/rapid.h"

struct sRGBColor : rapid::float4a
{
    constexpr sRGBColor() noexcept: rapid::float4a(0.f, 0.f, 0.f, 1.f) {}
    constexpr sRGBColor(float x) noexcept:
        rapid::float4a(x, x, x, 1.f) {}
    constexpr sRGBColor(float r, float g, float b) noexcept:
        rapid::float4a(r, g, b, 1.f) {}
    constexpr sRGBColor(float r, float g, float b, float a) noexcept:
        rapid::float4a(r, g, b, a) {}
    constexpr sRGBColor(int r, int g, int b) noexcept:
        rapid::float4a(r/255.f, g/255.f, b/255.f, 1.f) {}
    constexpr sRGBColor(int r, int g, int b, int a) noexcept:
        rapid::float4a(r/255.f, g/255.f, b/255.f, a/255.f) {}
    constexpr sRGBColor(const sRGBColor& c, float w):
        rapid::float4a(c.x, c.y, c.z, w) {}
    constexpr sRGBColor operator*(float s) const noexcept {
        return sRGBColor(x * s, y * s, z * s, w);
    }
};

struct LinearColor : rapid::float4a
{
    constexpr LinearColor() noexcept: rapid::float4a(0.f, 0.f, 0.f, 1.f) {}
    constexpr LinearColor(const sRGBColor& color) noexcept:
        rapid::float4a(linear(color.x), linear(color.y), linear(color.z), color.w) {}

    // https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_sRGB.txt
    constexpr float linear(float cs) const noexcept
    {
        cs = std::min(std::max(cs, 0.f), 1.f);
        if (cs <= 0.04045f)
            return cs / 12.92f;
        return pow((cs + 0.055f)/1.055f, 2.4f);
    }
};
